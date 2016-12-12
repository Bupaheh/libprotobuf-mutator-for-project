// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "protobuf_mutator.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <random>
#include <string>

#include "google/protobuf/message.h"

#include "field_instance.h"
#include "weighted_reservoir_sampler.h"

using google::protobuf::Descriptor;
using google::protobuf::EnumDescriptor;
using google::protobuf::EnumValueDescriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Message;
using google::protobuf::OneofDescriptor;
using google::protobuf::Reflection;

namespace protobuf_mutator {

namespace {

const int kMaxInitializeDepth = 32;

enum class Mutation {
  None,
  Add,     // Adds new field with default value.
  Mutate,  // Mutates field contents.
  Delete,  // Deletes field.

  // TODO(vitalybuka):
  // Clone,  // Adds new field with value copied from another field.
  // Copy,   // Copy values copied from another field.
  // Swap,   // Swap values of two fields.
};

// Flips random bit in the buffer.
void FlipBit(size_t size, uint8_t* bytes,
             ProtobufMutator::RandomEngine* random) {
  size_t bit = std::uniform_int_distribution<size_t>(0, size * 8 - 1)(*random);
  bytes[bit / 8] ^= (1u << (bit % 8));
}

// Flips random bit in the value.
template <class T>
T FlipBit(T value, ProtobufMutator::RandomEngine* random) {
  FlipBit(sizeof(value), reinterpret_cast<uint8_t*>(&value), random);
  return value;
}

// Return random integer from [0, count)
size_t GetRandomIndex(ProtobufMutator::RandomEngine* random, size_t count) {
  assert(count > 0);
  if (count == 1) return 0;
  return std::uniform_int_distribution<size_t>(0, count - 1)(*random);
}

struct CreateDefaultFieldTransformation {
  template <class T>
  void Apply(const FieldInstance& field) const {
    T value;
    field.GetDefault(&value);
    field.Create(value);
  }
};

struct DeleteFieldTransformation {
  template <class T>
  void Apply(const FieldInstance& field) const {
    field.Delete();
  }
};

// Selects random field and mutation from the given proto message.
class MutationSampler {
 public:
  MutationSampler(bool keep_initialized, float current_usage,
                  ProtobufMutator::RandomEngine* random, Message* message)
      : keep_initialized_(keep_initialized), random_(random), sampler_(random) {
    if (current_usage > kDeletionThreshold) {
      // Avoid adding new field and prefer deleting fields if we getting close
      // to the limit.
      add_weight_ *= 1 - current_usage;
      delete_weight_ *= current_usage;
    }
    Sample(message);
    assert(mutation() != Mutation::None);
  }

  // Returns selected field.
  const FieldInstance& field() const { return sampler_.selected().field; }

  // Returns selected mutation.
  Mutation mutation() const { return sampler_.selected().mutation; }

 private:
  void Sample(Message* message) {
    const Descriptor* descriptor = message->GetDescriptor();
    const Reflection* reflection = message->GetReflection();

    int field_count = descriptor->field_count();
    for (int i = 0; i < field_count; ++i) {
      const FieldDescriptor* field = descriptor->field(i);
      if (const OneofDescriptor* oneof = field->containing_oneof()) {
        // Handle entire oneof group on the first field.
        if (field->index_in_oneof() == 0) {
          sampler_.Try(
              add_weight_,
              {{message,
                oneof->field(GetRandomIndex(random_, oneof->field_count()))},
               Mutation::Add});
          if (const FieldDescriptor* field =
                  reflection->GetOneofFieldDescriptor(*message, oneof)) {
            if (field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE)
              sampler_.Try(kMutateWeight, {{message, field}, Mutation::Mutate});
            sampler_.Try(delete_weight_, {{message, field}, Mutation::Delete});
          }
        }
      } else {
        if (field->is_repeated()) {
          int field_size = reflection->FieldSize(*message, field);
          sampler_.Try(add_weight_, {{message, field,
                                      GetRandomIndex(random_, field_size + 1)},
                                     Mutation::Add});

          if (field_size) {
            size_t random_index = GetRandomIndex(random_, field_size);
            if (field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE)
              sampler_.Try(kMutateWeight,
                           {{message, field, random_index}, Mutation::Mutate});
            sampler_.Try(delete_weight_,
                         {{message, field, random_index}, Mutation::Delete});
          }
        } else {
          if (reflection->HasField(*message, field)) {
            if (field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE)
              sampler_.Try(kMutateWeight, {{message, field}, Mutation::Mutate});
            if ((!field->is_required() || !keep_initialized_))
              sampler_.Try(delete_weight_,
                           {{message, field}, Mutation::Delete});
          } else {
            sampler_.Try(add_weight_, {{message, field}, Mutation::Add});
          }
        }
      }

      if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
        if (field->is_repeated()) {
          const int field_size = reflection->FieldSize(*message, field);
          for (int j = 0; j < field_size; ++j)
            Sample(reflection->MutableRepeatedMessage(message, field, j));
        } else if (reflection->HasField(*message, field)) {
          Sample(reflection->MutableMessage(message, field));
        }
      }
    }
  }

  bool keep_initialized_ = false;
  const uint64_t kMutateWeight = 1000000;
  const float kDeletionThreshold = 0.5;

  // Adding and deleting are intrusive and expensive mutations, we'd like to do
  // them less often than field mutations.
  uint64_t add_weight_ = kMutateWeight / 10;
  uint64_t delete_weight_ = kMutateWeight / 10;

  ProtobufMutator::RandomEngine* random_;

  struct Result {
    Result() = default;
    Result(const FieldInstance& f, Mutation m) : field(f), mutation(m) {}

    FieldInstance field;
    Mutation mutation = Mutation::None;
  };
  WeightedReservoirSampler<Result, ProtobufMutator::RandomEngine> sampler_;
};

}  // namespace

class MutateTransformation {
 public:
  MutateTransformation(size_t allowed_growth, ProtobufMutator* mutator)
      : allowed_growth_(allowed_growth), mutator_(mutator) {}

  template <class T>
  void Apply(const FieldInstance& field) const {
    T value;
    field.Load(&value);
    Mutate(&value);
    field.Store(value);
  }

 private:
  void Mutate(int32_t* value) const { *value = mutator_->MutateInt32(*value); }

  void Mutate(int64_t* value) const { *value = mutator_->MutateInt64(*value); }

  void Mutate(uint32_t* value) const {
    *value = mutator_->MutateUInt32(*value);
  }

  void Mutate(uint64_t* value) const {
    *value = mutator_->MutateUInt64(*value);
  }

  void Mutate(float* value) const { *value = mutator_->MutateFloat(*value); }

  void Mutate(double* value) const { *value = mutator_->MutateDouble(*value); }

  void Mutate(bool* value) const { *value = mutator_->MutateBool(*value); }

  void Mutate(FieldInstance::Enum* value) const {
    value->index = mutator_->MutateEnum(value->index, value->count);
    assert(value->index < value->count);
  }

  void Mutate(std::string* value) const {
    *value = mutator_->MutateString(*value, allowed_growth_);
  }

  void Mutate(std::unique_ptr<Message>*) const { assert(!"Unexpected"); }

  size_t allowed_growth_;
  ProtobufMutator* mutator_;
};

ProtobufMutator::ProtobufMutator(uint32_t seed, bool keep_initialized)
    : keep_initialized_(keep_initialized), random_(seed) {}

void ProtobufMutator::Mutate(Message* message, size_t current_size,
                             size_t max_size) {
  assert(max_size);
  MutationSampler mutation(
      keep_initialized_, current_size / std::max<float>(current_size, max_size),
      &random_, message);

  size_t allowed_growth = (std::max(max_size, current_size) - current_size) / 4;

  switch (mutation.mutation()) {
    case Mutation::None:
      break;
    case Mutation::Add:
      mutation.field().Apply(CreateDefaultFieldTransformation());
      break;
    case Mutation::Mutate:
      mutation.field().Apply(MutateTransformation(allowed_growth, this));
      break;
    case Mutation::Delete:
      mutation.field().Apply(DeleteFieldTransformation());
      break;
    default:
      assert(!"unexpected mutation");
  }

  if (keep_initialized_ && !message->IsInitialized()) {
    InitializeMessage(message, kMaxInitializeDepth);
    assert(message->IsInitialized());
  }
}

void ProtobufMutator::InitializeMessage(Message* message, int max_depth) {
  assert(keep_initialized_);
  // It's pointless but possible to have infinite recursion of required
  // messages.
  assert(max_depth);
  const Descriptor* descriptor = message->GetDescriptor();
  const Reflection* reflection = message->GetReflection();
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const FieldDescriptor* field = descriptor->field(i);
    if (field->is_required() && !reflection->HasField(*message, field))
      FieldInstance(message, field).Apply(CreateDefaultFieldTransformation());

    if (max_depth > 0 &&
        field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      if (field->is_repeated()) {
        const int field_size = reflection->FieldSize(*message, field);
        for (int j = 0; j < field_size; ++j) {
          Message* nested_message =
              reflection->MutableRepeatedMessage(message, field, j);
          if (!nested_message->IsInitialized())
            InitializeMessage(nested_message, max_depth - 1);
        }
      } else if (reflection->HasField(*message, field)) {
        Message* nested_message = reflection->MutableMessage(message, field);
        if (!nested_message->IsInitialized())
          InitializeMessage(nested_message, max_depth - 1);
      }
    }
  }
}

int32_t ProtobufMutator::MutateInt32(int32_t value) {
  return FlipBit(value, &random_);
}

int64_t ProtobufMutator::MutateInt64(int64_t value) {
  return FlipBit(value, &random_);
}

uint32_t ProtobufMutator::MutateUInt32(uint32_t value) {
  return FlipBit(value, &random_);
}

uint64_t ProtobufMutator::MutateUInt64(uint64_t value) {
  return FlipBit(value, &random_);
}

float ProtobufMutator::MutateFloat(float value) {
  return FlipBit(value, &random_);
}

double ProtobufMutator::MutateDouble(double value) {
  return FlipBit(value, &random_);
}

bool ProtobufMutator::MutateBool(bool value) {
  return std::uniform_int_distribution<uint8_t>(0, 1)(random_);
}

size_t ProtobufMutator::MutateEnum(size_t index, size_t item_count) {
  return (index +
          std::uniform_int_distribution<uint8_t>(1, item_count - 1)(random_)) %
         item_count;
}

std::string ProtobufMutator::MutateString(const std::string& value,
                                          size_t allowed_growth) {
  std::string result = value;
  int min_diff = result.empty() ? 0 : -1;
  int max_diff = allowed_growth ? 1 : 0;
  int diff = std::uniform_int_distribution<int>(min_diff, max_diff)(random_);
  if (diff == -1) {
    result.erase(GetRandomIndex(&random_, result.size()), 1);
    return result;
  }

  if (diff == 1) {
    size_t index = GetRandomIndex(&random_, result.size() + 1);
    result.insert(result.begin() + index, '\0');
    FlipBit(1, reinterpret_cast<uint8_t*>(&result[index]), &random_);
    return result;
  }

  if (!result.empty())
    FlipBit(result.size(), reinterpret_cast<uint8_t*>(&result[0]), &random_);
  return result;
}

}  // namespace protobuf_mutator
