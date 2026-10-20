using mv.impact.acquire.helper;
using System;
using System.Collections.Generic;
using System.Text;

namespace mv.impact.acquire.examples
{
    internal class UserSuppliedMemory
    {
        #region public methods ------------------------------------------------

        public static void checkCaptureBufferAddress(Request pRequest, bool boShouldContainUserSuppliedMemory, List<UserSuppliedHeapBuffer> buffers)
        {
            if (boShouldContainUserSuppliedMemory && (pRequest.imageMemoryMode.read() != TRequestImageMemoryMode.rimmUser))
            {
                Console.WriteLine("Request number %d is supposed to contain user supplied memory, but claims that it doesn't.", pRequest.number);
            }
            else if (!boShouldContainUserSuppliedMemory)
            {
                if (pRequest.imageMemoryMode.read() == TRequestImageMemoryMode.rimmUser)
                {
                    Console.WriteLine("Request number %d is supposed NOT to contain user supplied memory, but claims that it does.", pRequest.number);
                }
                return;
            }

            IntPtr pAddr = pRequest.imageData.read();
            int vSize = buffers.Count;
            for (int i = 0; i < vSize; i++)
            {
                if (pAddr == buffers[i].pointer)
                {
                    // found the buffer that has been assigned by the user
                    return;
                }
            }

            StringBuilder s = new StringBuilder();
            s.AppendFormat("ERROR: A buffer has been returned, that doesn't match any of the buffers assigned as user memory in request number {0}. Buffer got: {1}\nbuffers allocated:\n", pRequest.number, pAddr);
            for (int j = 0; j < vSize; j++)
            {
                s.AppendFormat("[{0}]: 0x{1}\n", j, buffers[j].pointer);
            }
            Console.WriteLine(s.ToString());
        }

        public static TDMR_ERROR createCaptureBuffers(FunctionInterface fi, List<UserSuppliedHeapBuffer> buffers, int bufferSize, int bufferAlignment)
        {
            freeCaptureBuffers(fi, buffers);

            TDMR_ERROR result = TDMR_ERROR.DMR_NO_ERROR;
            int requestCnt = fi.requestCount;
            for (int i = 0; i < requestCnt; i++)
            {
                try
                {
                    Request pRequest = fi.getRequest(i);
                    UserSuppliedHeapBuffer pBuffer = new UserSuppliedHeapBuffer(bufferSize, bufferAlignment);
                    buffers.Add(pBuffer);
                    if ((result = (TDMR_ERROR)pRequest.attachUserBuffer(pBuffer.pointer, pBuffer.size)) != TDMR_ERROR.DMR_NO_ERROR)
                    {
                        freeCaptureBuffers(fi, buffers);
                        Console.WriteLine("An error occurred while attaching a buffer to request number {0}: {1}.", i, ImpactAcquireException.getErrorCodeAsString(result));
                        return result;
                    }
                }
                catch (ImpactAcquireException e)
                {
                    freeCaptureBuffers(fi, buffers);
                    Console.WriteLine("An error occurred while attaching a buffer to request number {0}: {1}.", i, e.errorCodeAsString);
                    return (TDMR_ERROR)e.errorCode;
                }
            }
            return result;
        }

        public static void freeCaptureBuffers(FunctionInterface fi, List<UserSuppliedHeapBuffer> buffers)
        {
            int requestCnt = fi.requestCount;
            TDMR_ERROR result = TDMR_ERROR.DMR_NO_ERROR;
            for (int i = 0; i < requestCnt; i++)
            {
                try
                {
                    Request pRequest = fi.getRequest(i);
                    if (pRequest.imageMemoryMode.read() != TRequestImageMemoryMode.rimmUser)
                    {
                        continue;
                    }

                    if ((result = (TDMR_ERROR)pRequest.detachUserBuffer()) != TDMR_ERROR.DMR_NO_ERROR)
                    {
                        Console.WriteLine("An error occurred while detaching a buffer from request number {0}: {1}.", i, ImpactAcquireException.getErrorCodeAsString(result));
                        continue;
                    }
                }
                catch (ImpactAcquireException e)
                {
                    Console.WriteLine("An error occurred while changing the mode of request number {0}: {1}.", i, e.errorCodeAsString);
                }
            }
            buffers.Clear();
        }

        #endregion
    }
}