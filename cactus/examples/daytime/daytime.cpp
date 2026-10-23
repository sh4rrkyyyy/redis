#include <cactus/cactus.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_bool(server, false, "Start daytime server");
DEFINE_bool(client, false, "Start daytime client");
DEFINE_string(address, "127.0.0.1", "Server address");
DEFINE_uint32(port, 10'013, "Server port");

std::string GetTime() {
    time_t rawtime;
    time(&rawtime);

    tm timeinfo;
    localtime_r(&rawtime, &timeinfo);

    std::string s(1024, '\0');
    s.resize(strftime(s.data(), s.size(), "%c", &timeinfo));
    return s;
}

void RunClient() {
    auto port = static_cast<uint16_t>(FLAGS_port);
    cactus::SocketAddress address{FLAGS_address, port};

    LOG(INFO) << "Connecting to server at " << address;

    auto conn = cactus::DialTCP(address);

    LOG(INFO) << "Connected from " << conn->LocalAddress();

    auto date = conn->ReadAllToString();

    LOG(INFO) << date;
}

void RunServer() {
    cactus::SocketAddress addr(FLAGS_address, FLAGS_port);

    auto lsn = cactus::ListenTCP(addr);
    LOG(INFO) << "Listening for client at " << lsn->Address();

    while (true) {
        auto client = lsn->Accept();
        LOG(INFO) << "Accepted client from " << client->RemoteAddress();

        client->Write(cactus::View(GetTime()));

        client->Close();
    }
}

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();
    FLAGS_logtostderr = true;

    cactus::Scheduler scheduler;
    scheduler.Run([&] {
        if (FLAGS_server) {
            RunServer();
        } else if (FLAGS_client) {
            RunClient();
        } else {
            LOG(ERROR) << "Must specify --client or --server";
        }
    });
}
