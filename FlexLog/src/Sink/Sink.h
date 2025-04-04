#pragma once

#include "Common.h"
#include "Format/Format.h"
#include "Message.h"

namespace FlexLog
{
    class Sink
    {
    public:
        Sink() = default;
        virtual ~Sink() = default;

        virtual void Output(const Message& msg, const Format& format) = 0;
        virtual void Flush() {}
    };
}
