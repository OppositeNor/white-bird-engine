#ifndef __WBE_LOGGING_MANAGER_TEST_HH__
#define __WBE_LOGGING_MANAGER_TEST_HH__

#include "core/core_utils.hh"
#include "core/logging/log.hh"
#include "core/logging/logging_manager.hh"
#include "global/global.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>
#include <ostream>

namespace WBE = WhiteBirdEngine;

namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(WBE_TEST_LABEL_1)
WBE_DEFINE_LABEL(WBE_TEST_LABEL_2)
WBE_DEFINE_LABEL(WBE_TEST_LABEL_3)
}

class LogTestMock : public WBE::Log {
public:

    LogTestMock(WBE::ChannelID p_channel_id, std::ostream& p_ostream)
        : channel_id(p_channel_id), ostream(&p_ostream) {
        *ostream << "Construct " << WBE::get_label_name(channel_id) << std::endl;
    }

    virtual ~LogTestMock() override {
        *ostream << "Destruct " << WBE::get_label_name(channel_id) << std::endl;
    }

    WBE::ChannelID channel_id;
    std::ostream* ostream;

    virtual WBE::ChannelID get_channel() const override {
        return channel_id;
    }

    virtual void message(const std::string& p_str) override {
        *ostream << "Message " << WBE::get_label_name(channel_id) << " " << p_str << std::endl;
    }


    virtual void warning(const std::string& p_str) override {
        *ostream << "Warning " << WBE::get_label_name(channel_id) << " " << p_str << std::endl;
    }

    virtual void error(const std::string& p_str) override {
        *ostream << "Error " << WBE::get_label_name(channel_id) << " " << p_str << std::endl;
    }
};

TEST(WBELoggingManagerTest, GetLogConstructDestruct) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::stringstream ss;
    {
        WBE::LoggingManager<LogTestMock, std::ostream> logging_manager(ss);
        WBE::Log* log1 = logging_manager.get_log(WBE::WBE_TEST_LABEL_1);
        ASSERT_NE(log1, nullptr);
        ASSERT_EQ(log1, logging_manager.get_log(WBE::WBE_TEST_LABEL_1));
        WBE::Log* log2 = logging_manager.get_log(WBE::WBE_TEST_LABEL_2);
        ASSERT_NE(log2, nullptr);
        ASSERT_NE(log2, log1);
    }
    ASSERT_NE(ss.str().find("Construct WBE_TEST_LABEL_1\n"), std::string::npos);
    ASSERT_EQ(ss.str().find("Construct WBE_TEST_LABEL_1\n"), ss.str().rfind("Construct WBE_TEST_LABEL_1\n"));
    ASSERT_NE(ss.str().find("Construct WBE_TEST_LABEL_2\n"), std::string::npos);
    ASSERT_EQ(ss.str().find("Construct WBE_TEST_LABEL_2\n"), ss.str().rfind("Construct WBE_TEST_LABEL_2\n"));
    ASSERT_NE(ss.str().find("Destruct WBE_TEST_LABEL_1\n"), std::string::npos);
    ASSERT_EQ(ss.str().find("Destruct WBE_TEST_LABEL_1\n"), ss.str().rfind("Destruct WBE_TEST_LABEL_1\n"));
    ASSERT_NE(ss.str().find("Destruct WBE_TEST_LABEL_2\n"), std::string::npos);
    ASSERT_EQ(ss.str().find("Destruct WBE_TEST_LABEL_2\n"), ss.str().rfind("Destruct WBE_TEST_LABEL_2\n"));
}

#endif
