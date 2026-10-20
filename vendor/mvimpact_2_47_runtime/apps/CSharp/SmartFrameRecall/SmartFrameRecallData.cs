using System;

namespace mv.impact.acquire.examples
{
    public struct SmartFrameRecallData
    {
        public Int64 timestamp_us_;
        public int x_;
        public int y_;
        public int w_;
        public int h_;
        public uint identifier_;
        public SmartFrameRecallData(Int64 timestamp_us, int x, int y, int w, int h, uint identifier)
        {
            timestamp_us_ = timestamp_us;
            x_ = x;
            y_ = y;
            w_ = w;
            h_ = h;
            identifier_ = identifier;
        }
    }
}
