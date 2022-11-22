//
//  LogIo.h
//  core
//
//  Created by issac on 2021/1/11.
//

#ifndef CORE_GLOG_IO_H_
#define CORE_GLOG_IO_H_
#include "GlogPredef.h"

using namespace std;
namespace glog {

enum class HeaderMismatchReason : uint8_t {
    SizeTooSmall = 0,
    ReadFail = 1,
    MagicNumMismatch,
    VersionCodeMismatch,
    ProtoNameMismatch,
    SyncMarkerMismatch,
    None
};

extern format::ModeSet toModeSet(GlogByte_t value);
extern GlogByte_t fromModeSet(format::ModeSet st);

extern uint32_t logStoreSize(GlogBufferLength_t logLength, bool cipher);
extern uint8_t logHeaderSize(bool cipher);

} // namespace glog
#endif /* CORE_GLOG_IO_H_ */
