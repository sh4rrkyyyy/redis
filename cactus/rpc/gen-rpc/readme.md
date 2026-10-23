# gen-rpc

Вам нужно реализовать плагин к `protoc`, который будет генерировать
код для работы с rpc.

Для каждого сервиса в proto файле ваш плагин должен генерировать два класса.

```proto
service TestService {
    rpc Search (TestRequest) returns (TestResponse) {}
}
```

```c++
class TestServiceClient {
public:
    explicit TestServiceClient(std::unique_ptr<cactus::IRpcChannel> channel)
        : channel_{std::move(channel)} {
    }

    // Внутри вызывает метод CallMethod у сhannel.
    TestResponse Search(const TestRequest& req);

private:
    std::unique_ptr<cactus::IRpcChannel> channel_;
};

class TestServiceHandler : public cactus::IRpcService {
public:
    // Смотрит на method->index() и вызывает соответствующий метод.
    // Сообщения даункастить static_cast-ом.
    // Неправильные входные данные можно считать UB.
    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    const google::protobuf::Message& request,
                    google::protobuf::Message* response) override;

    // Виртуальный метод для реализации пользовательским кодом.
    virtual void Search(const TestRequest& req, TestResponse* rsp) = 0;

    // Реализация IRpcService. Нужно просто вернуть TestService::descriptor();
    // Класс TestService уже генерирует protoc.
    const google::protobuf::ServiceDescriptor* ServiceDescriptor() override;
};
```

Плагин находится в файле rpc_generator.cpp. С помошью `GeneratorContext::OpenForInsert`
он должен дописывать в `pb.cc` и `pb.h` файлы свой код. В файле уже есть пример работы с этим api.

Чтобы тестировать код, запускайте `test_cactus_rpc_gen` и смотрите на ошибки компиляции и тестов.
При его сборке происходит следующее:
* Собирается плагин `protoc_gen_rpc` из файлов `rpc_generator.cpp` и `main.cpp`
* `protoc` вместе с плагином `protoc_gen_rpc` запускается на нескольких .proto файлах
* Для каждого .proto файла с помощью вашего плагина генерируется пара из .pb.h и .pb.cc
файлов по пути `<build_директория>/tools/cactus/rpc`
* Собирается `test_cactus_rpc_gen` из файла `test.cpp`

## Примечение
Ваш генератор должен быть универсальным, то есть он не должен зависеть от конкретного
набора сообщений и сервисов в .proto файле. Чтобы проверить ваше решение на это свойство
в данной задаче есть приватные тесты.
