# Структурный xml фаззинг

## Сборка

Install prerequisites:

```sh
sudo apt-get update
sudo apt-get install protobuf-compiler libprotobuf-dev binutils cmake \
  ninja-build liblzma-dev libz-dev pkg-config autoconf libtool
```

Compile and test everything:

```sh
mkdir build
cd build
cmake .. -GNinja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
ninja check
```

Clang is only needed for libFuzzer integration. <BR>
By default, the system-installed version of
[protobuf](https://github.com/google/protobuf) is used.  However, on some
systems, the system version is too old.  You can pass
`LIB_PROTO_MUTATOR_DOWNLOAD_PROTOBUF=ON` to cmake to automatically download and
build a working version of protobuf.

## Запуск примера

Таргетом является xml -> json конвертер: [xml2json](https://github.com/Cheedoong/xml2json).

В качестве [прото-схемы](examples/xml/xml.proto) выступает упрощенная и модифицированная схема [maven POM](https://maven.apache.org/pom.html).

После [сборки проекта](#сборка) для компиляции примера используйте команду:
 
```sh
cmake --build build --target expat_example
```

Запуск фаззера:

```sh
build/examples/expat/expat_example
```

## Описание прото схемы

Корнем прото-схемы выступает сообщение `Input`.

Поля-сообщения транслируются в дочерние xml-элементы, имя тега совпадает с именем прото-поля.

Строковые константы представляются прото-перечислениями.

Поля-примитивы и перечисления транслируются в текущий элемент без создания дочернего.

Атрибуты представляются полем-сообщением с именем `xml_attributes`.

#### Поддержано:
- required поля
- optional поля
- repeated поля
- строки
- числовые примитивы: `int32`, `uint32`, `int64`, `uint64`
- перечисления

#### Пример:

[Прото-описание](examples/xml/xml.proto) -> [пример сгенерированного xml](examples/xml/output-example.xml)