#include "Logging.h"

int main()
{
    auto& logManager = FlexLog::LogManager::GetInstance();
    logManager.Initialize();

    auto sink = std::make_shared<FlexLog::ConsoleSink>();

    std::vector<FlexLog::Logger*> loggers;
    for (size_t i = 0; i < 10; ++i)
    {
        auto& logger = logManager.RegisterLogger("logger" + std::to_string(i));
        logger.SetLevel(FlexLog::Level::Trace);
        logger.GetFormat().SetLogFormat(FlexLog::LogFormat::Splunk);
        logger.RegisterSink(sink);

        loggers.push_back(&logger);
    }

    for (int i = 0; i < 1000; ++i)
    {
        for (auto& logger : loggers)
        {
            if (logger)
                logger->Info("This is an INFO message");
        }
    }

    logManager.Shutdown();
    return 0;
}
