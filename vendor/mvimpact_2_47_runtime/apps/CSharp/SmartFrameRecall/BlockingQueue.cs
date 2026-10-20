using System.Collections.Generic;
using System.Threading;

namespace mv.impact.acquire.examples
{
    public class BlockingQueue<T> where T : class
    {
        private bool closing_;
        private readonly Queue<T> queue_ = new Queue<T>();

        public int Count
        {
            get
            {
                lock (queue_)
                {
                    return queue_.Count;
                }
            }
        }
        public BlockingQueue()
        {
            lock (queue_)
            {
                closing_ = false;
                Monitor.PulseAll(queue_);
            }
        }
        public bool Enqueue(T item)
        {
            lock (queue_)
            {
                if (closing_ || null == item)
                {
                    return false;
                }
                queue_.Enqueue(item);
                if (queue_.Count == 1)
                {
                    // wake up any blocked dequeue
                    Monitor.PulseAll(queue_);
                }
                return true;
            }
        }
        public void Close()
        {
            lock (queue_)
            {
                if (!closing_)
                {
                    closing_ = true;
                    queue_.Clear();
                    Monitor.PulseAll(queue_);
                }
            }
        }
        public bool TryDequeue(out T value, int timeout)
        {
            lock (queue_)
            {
                while (queue_.Count == 0)
                {
                    if (closing_ || (timeout < Timeout.Infinite) || !Monitor.Wait(queue_, timeout))
                    {
                        value = default(T);
                        return false;
                    }
                }
                value = queue_.Dequeue();
                return true;
            }
        }
        public bool TryDequeue(out T value)
        {
            return TryDequeue(out value, Timeout.Infinite);
        }
        public void Clear()
        {
            lock (queue_)
            {
                queue_.Clear();
                Monitor.Pulse(queue_);
            }
        }
    }
}
