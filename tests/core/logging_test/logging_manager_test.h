#ifndef __WBE_LOGGING_MANAGER_TEST_H__
#define __WBE_LOGGING_MANAGER_TEST_H__

#include "core/logging/log.h"
#include "core/logging/logging_manager.h"
#include "utils/defs.h"
#include <gtest/gtest.h>
#include <ostream>

namespace WBE = WhiteBirdEngine;

namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(WBE_TEST_LABEL_1)
WBE_DEFINE_LABEL(WBE_TEST_LABEL_2)
WBE_DEFINE_LABEL(WBE_TEST_LABEL_3)
}

template <WBE::ChannelID CHANNEL>
class LogTestMock : public WBE::Log {
public:

    LogTestMock(std::ostream& p_ostream)
        : ostream(&p_ostream) {
        *ostream << "Construct " << WBE_LABEL_NAME(CHANNEL) << std::endl;
    }

    virtual ~LogTestMock() override {
        *ostream << "Destruct " << WBE_LABEL_NAME(CHANNEL) << std::endl;
    }

    std::ostream* ostream;

    virtual WBE::ChannelID get_current_channel() const override {
        return CHANNEL;
    }

    virtual void message(const std::string& p_str) override {
        *ostream << "Message " << WBE_LABEL_NAME(CHANNEL) << " " << p_str << std::endl;
    }


    virtual void warning(const std::string& p_str) override {
        *ostream << "Warning " << WBE_LABEL_NAME(CHANNEL) << " " << p_str << std::endl;
    }

    virtual void error(const std::string& p_str) override {
        *ostream << "Error " << WBE_LABEL_NAME(CHANNEL) << " " << p_str << std::endl;
    }

};

TEST(WBELoggingManagerTest, GetLogConstructDestruct) {
    std::stringstream ss;
    {
        WBE::LoggingManager<LogTestMock, std::ostream> logging_manager(ss);
        WBE::Log* log1 = logging_manager.get_log<WBE::WBE_TEST_LABEL_1>();
        ASSERT_NE(log1, nullptr);
        ASSERT_EQ(log1, logging_manager.get_log<WBE::WBE_TEST_LABEL_1>());
        WBE::Log* log2 = logging_manager.get_log<WBE::WBE_TEST_LABEL_2>();
        ASSERT_NE(log2, nullptr);
        ASSERT_NE(log2, log1);
    }
    ASSERT_EQ(ss.str(), std::string(
        "Construct WBE_TEST_LABEL_1\n"
        "Construct WBE_TEST_LABEL_2\n"
        "Destruct WBE_TEST_LABEL_1\n"
        "Destruct WBE_TEST_LABEL_2\n"
    ));
}

#endif
