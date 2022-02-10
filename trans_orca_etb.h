/*
 *  trans_orca_etb.h - The etb dump class.
 *
 *  Copyright (C) 2020 UNISOC Communications Inc.
 *
 *  History:
 *
 *  2020-3-10 Chunlan Wang
 *  Initial version.
*/

#ifndef TRANS_ORCA_ETB_H_
#define TRANS_ORCA_ETB_H_

#include "cp_dump.h"
#include "timer_mgr.h"

#include "cp_log_cmn.h"
#include "cp_stat_hdl.h"
#include "def_config.h"
#include "diag_stream_parser.h"
#include "modem_utils.h"


class LogFile;
class WanModemLogHandler;


class TransOrcaEtb : public TransCpDump {
  public:

    TransOrcaEtb(TransactionManager* tmgr,
                  WanModemLogHandler* subsys, CpStorage& cp_stor,
                  const struct tm& lt, CpStateHandler::CpEvent evt,
                  const char* prefix);

    TransOrcaEtb(const TransOrcaEtb&) = delete;

    ~TransOrcaEtb();

    TransOrcaEtb& operator = (const TransOrcaEtb&) = delete;

    // Transaction::execute()
    int execute() override;
    // Transaction::cancel()
    void cancel() override;

    // TransDiagDevice::process()
    bool process(DataBuffer& buffer) override;

   private:
    int start_dump();
    bool check_etb_ending(DataBuffer& buffer);
    /*
     *   notify_stor_inactive - callback when external storage umounted.
     */
    static void notify_stor_inactive(void* client, unsigned priority);
    static void dump_etb_read_check(void* param);

   private:
    struct tm time_;
    TimerManager::Timer* m_timer;
    DiagStreamParser parser_;
    const char* prefix_;
    CpStateHandler::CpEvent evt_;
    LogString dump_path_;
    WanModemLogHandler* subsys_;
    // Whether data are received in the most recent time window
    bool dump_ongoing_;
    // The number of adjacent data silent windows
    int silent_wins_;
    void* client_;
};
#endif

