//-----------------------------------------------------------------------------
// (C) Copyright 2005 - 2022 by MATRIX VISION GmbH
//
// This software is provided by MATRIX VISION GmbH "as is"
// and any express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular purpose
// are disclaimed.
//
// In no event shall MATRIX VISION GmbH be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused and
// on any theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use of
// this software, even if advised of the possibility of such damage.

//-----------------------------------------------------------------------------
#ifndef MVIMPACT_ACQUIRE_HELPER_H
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define MVIMPACT_ACQUIRE_HELPER_H MVIMPACT_ACQUIRE_HELPER_H
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//-----------------------------------------------------------------------------
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>

#ifndef WRAP_ANY
#   include <atomic>
#   include <cassert>
#   include <chrono>
#   include <condition_variable>
#   include <memory>
#   include <mutex>
#   include <queue>
#   include <thread>
#endif // #ifdef WRAP_ANY

#ifdef _MSC_VER
#   pragma push_macro("max") // otherwise we can't work with the 'numeric_limits' template here as Windows defines a macro 'max'
#   undef max
#endif // #ifdef _MSC_VER

#if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) && (!defined(CPP_STANDARD_VERSION) || ( defined(CPP_STANDARD_VERSION) && (CPP_STANDARD_VERSION < 11)))
#   error "Combining the mvIMPACT_acquire_helper.h and mvIMPACT_acquire_display.h requires 'CPP_STANDARD_VERSION' being defined and set to a minimal value of 11! See documentation of the class mvIMPACT::acquire::helper::RequestProvider for details."
#endif // #if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) && (!defined(CPP_STANDARD_VERSION) || ( defined(CPP_STANDARD_VERSION) && (CPP_STANDARD_VERSION < 11)))

namespace mvIMPACT
{
namespace acquire
{
namespace helper
{

//-----------------------------------------------------------------------------
/// \brief A thread-safe queue.
/**
 * This class provides a thread-safe queue implementation with all the operations
 * typically needed within a multi-threaded environment.
 *
 * \note This class requires a C++11 compliant compiler!
 *
 * \since 2.33.0
 * \ingroup CommonInterface
 */
template <class T>
class ThreadSafeQueue final
//-----------------------------------------------------------------------------
{
public:
    /// \brief Creates a new \b mvIMPACT::acquire::helper::ThreadSafeQueue instance.
    explicit ThreadSafeQueue(
        /// [in] The maximum number of elements this queue shall be allowed to store until another push operation will fail.
        typename std::queue<T>::size_type queueSizeMax = std::numeric_limits<typename std::queue<T>::size_type>::max() ) :
        queue_(), isLocked_( false ), wakeupRequestPending_( false ), queueSizeMax_( queueSizeMax ), mutex_(), conditionVariable_() {}
    /// \brief Copy-constructor (<b>deleted</b>)
    /**
     * Objects of this type shall not be copy-constructed!
     */
    ThreadSafeQueue( const ThreadSafeQueue& src ) = delete;
    /// \brief assignment operator (<b>deleted</b>)
    /**
     * Objects of this type shall not be assigned!
     */
    ThreadSafeQueue& operator=( const ThreadSafeQueue& rhs ) = delete;

    //-----------------------------------------------------------------------------
    /// \brief Defines valid queue result values.
    /**
     * These enumeration defines valid values that might be returned from some of the
     * queue operations defined by the surrounding \b mvIMPACT::acquire::helper::ThreadSafeQueue
     * class.
     */
    enum TQueueResult
    //-----------------------------------------------------------------------------
    {
        /// \brief The operation on the queue returned successfully.
        qrNoError = 0,
        /// \brief The operation on the queue could not be executed as the queue is currently full thus contains the maximum number of allowed elements.
        qrFull,
        /// \brief The operation on the queue could not be executed as the queue is currently locked.
        qrLocked
    };

    /// \brief Clears the queue. Discards all data.
    void clear( void )
    {
        while( pop( 0, nullptr ) ) {};
    }
    /// \brief Returns the maximum number of elements this queue can store.
    /**
     * When trying to push more elements into the queue than returned by this function the next <b>mvIMPACT::acquire::helper::ThreadSafeQueue::push</b>
     * call will fail until at least one element has been extracted before. The maximum number of elements a queue can store
     * can be specified when constructing the queue.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::clear \n
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::getCurrentSize \n
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::pop
     *
     * \return The maximum number of elements this queue can store.
     */
    typename std::queue<T>::size_type getMaxSize( void ) const
    {
        return queueSizeMax_;
    }
    /// \brief Returns the current number of elements stored by this queue.
    /**
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::getMaxSize
     *
     * \return The current number of elements stored by this queue.
     */
    typename std::queue<T>::size_type getCurrentSize( void ) const
    {
        std::lock_guard<std::mutex> scopeLock( mutex_ );
        return queue_.size();
    }
    /// \brief Checks if this queue is currently full.
    /**
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::getMaxSize
     *
     * \return
     * - true if the queue is currently full
     * - false otherwise
     */
    bool isFull( void ) const
    {
        std::lock_guard<std::mutex> scopeLock( mutex_ );
        return queue_.size() == queueSizeMax_;
    }
    /// \brief Locks the queue.
    /**
     * While a queue is locked subsequent calls to \b mvIMPACT::acquire::helper::ThreadSafeQueue::push will
     * fail and will return \b mvIMPACT::acquire::helper::ThreadSafeQueue::qrLocked.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::getMaxSize, \n
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::push
     *
     * \return
     * - true if the queue was locked already
     * - false otherwise
     */
    bool lock( void )
    {
        return isLocked_.exchange( true );
    }
    /// \brief Unlocks the queue.
    /**
     * While a queue is locked subsequent calls to \b mvIMPACT::acquire::helper::ThreadSafeQueue::push will
     * fail and will return \b mvIMPACT::acquire::helper::ThreadSafeQueue::qrLocked.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::getMaxSize, \n
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::push
     *
     * \return
     * - true if the queue was unlocked already
     * - false otherwise
     */
    bool unlock( void )
    {
        return isLocked_.exchange( false );
    }
    /// \brief Gets a copy of the oldest element from the queue.
    /**
     * Gets a copy of the \e oldest element in the queue and the same element that is popped out from the queue
     * when mvIMPACT::acquire::helper::ThreadSafeQueue::pop is called.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::pop, \n
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::push
     *
     * \return
     * - true if the queue contains at least one element
     * - false if the queue is empty
     */
    bool front(
        /// [in,out] A pointer to the storage location that shall receive a copy of the oldest element currently stored in the queue. Can be \a nullptr if a caller just wants
        /// to check if there is at least one element stored be the queue right now.
        T* pData = nullptr ) const
    {
        std::lock_guard<std::mutex> scopeLock( mutex_ );
        const bool boResult = !queue_.empty();
        if( boResult && pData )
        {
            *pData = queue_.front();
        }
        return boResult;
    }
    /// \brief Adds a new copy of an element to the queue.
    /**
     * Adds a new copy of an element to the queue.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::front, \n
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::pop
     *
     * \return
     * - mvIMPACT::acquire::helper::ThreadSafeQueue::qrFull if the queue contains the maximum number of elements already. The new element will \b NOT be added then
     * - mvIMPACT::acquire::helper::ThreadSafeQueue::qrLocked if the queue is currently in a locked state. The new element will \b NOT be added then
     * - mvIMPACT::acquire::helper::ThreadSafeQueue::qrNoError otherwise
     */
    TQueueResult push(
        /// [in] A \a const \a reference to the element to store in the queue. The object must be copy-constructible as this operation
        /// will create a copy of the object.
        const T& t )
    {
        std::lock_guard<std::mutex> scopeLock( mutex_ );
        if( queue_.size() >= queueSizeMax_ )
        {
            return qrFull;
        }

        if( isLocked_.load() )
        {
            return qrLocked;
        }

        queue_.push( t );
        conditionVariable_.notify_one();
        return qrNoError;
    }
    /// \brief Removes the oldest element from the queue, effectively reducing its size by one.
    /**
     * Removes the oldest element from the queue, effectively reducing its size by one.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::front, \n
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::push
     *
     * \return
     * - true if the queue did contain at least one element
     * - false if the queue was empty
     */
    template<class Rep, class Period>
    bool pop(
        /// [in] The maximum time to wait for incoming data if the queue currently does not contain any data to pick up.
        const std::chrono::duration<Rep, Period>& rel_time,
        /// [in,out] A pointer to the storage location that shall receive a copy of the oldest element currently stored in the queue. Can be \a nullptr if a caller just wants
        /// to remove an element from the queue but is not actually interested in it.
        T* pData = nullptr )
    {
        std::unique_lock<std::mutex> scopeLock( mutex_ );
        return extractData( conditionVariable_.wait_for( scopeLock, rel_time, [this] { return !queue_.empty() || wakeupRequestPending_.load(); } ), pData );
    }
    /// \brief Removes the oldest element from the queue, effectively reducing its size by one.
    /**
     * Removes the oldest element from the queue, effectively reducing its size by one.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::front, \n
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::push
     *
     * \return
     * - true if the queue did contain at least one element
     * - false if the queue was empty
     */
    bool pop(
        /// [in] The maximum time (in mill-seconds) to wait for incoming data if the queue currently does not contain any data to pick up.
        unsigned int timeout_ms,
        /// [in,out] A pointer to the storage location that shall receive a copy of the oldest element currently stored in the queue. Can be \a nullptr if a caller just wants
        /// to remove an element from the queue but is not actually interested in it.
        T* pData = nullptr )
    {
        return pop( std::chrono::milliseconds( timeout_ms ), pData );
    }
    /// \brief Removes the oldest element from the queue, effectively reducing its size by one.
    /**
     * Removes the oldest element from the queue, effectively reducing its size by one.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::front, \n
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::push
     *
     * \return
     * - true if the queue did contain at least one element
     * - false if the queue was empty
     */
    bool pop(
        /// [in,out] A pointer to the storage location that shall receive a copy of the oldest element currently stored in the queue. Can be \a nullptr if a caller just wants
        /// to remove an element from the queue but is not actually interested in it.
        T* pData = nullptr )
    {
        std::unique_lock<std::mutex> scopeLock( mutex_ );
        conditionVariable_.wait( scopeLock, [this] { return !queue_.empty() || wakeupRequestPending_.load(); } );
        return extractData( true, pData );
    }
    /// \brief Terminates a single wait operation currently pending from another thread without delivering data.
    /**
     * If one or multiple thread(s) are currently executing a \b mvIMPACT::acquire::helper::ThreadSafeQueue::pop operation \b ONE of these
     * threads will get signaled and the call will return.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::ThreadSafeQueue::pop
     */
    void terminateWait( void )
    {
        {
            std::lock_guard<std::mutex> scopeLock( mutex_ );
            wakeupRequestPending_ = true;
        }
        conditionVariable_.notify_one();
    }
private:
    bool extractData( const bool boWaitResult, T* pData )
    {
        if( boWaitResult )
        {
            if( !queue_.empty() )
            {
                if( pData )
                {
                    *pData = std::move( queue_.front() );
                }
                queue_.pop();
                return true;
            }
            else if( wakeupRequestPending_.load() )
            {
                wakeupRequestPending_ = false;
            }
        }
        return false;
    }

    std::queue<T> queue_;
    std::atomic<bool> isLocked_;
    std::atomic<bool> wakeupRequestPending_;
    typename std::queue<T>::size_type queueSizeMax_;
    mutable std::mutex mutex_;
    std::condition_variable conditionVariable_;
};

//-----------------------------------------------------------------------------
/// \brief A helper class that can be used to implement a simple continuous acquisition from a device.
/**
 * This class is meant to provide a very convenient way of capturing data continuously from
 * a device. All buffer handling is done internally. An application simply needs to start and stop
 * the acquisition and can pick up data in between.
 *
 * Picking up data can be done in 2 ways:
 * - Passing an arbitrary function accepting any desired number of parameters to on of the overloads of the
 *   function \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStart
 * - Calling \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStart with \b NO parameters
 *   and then \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest consecutively
 *
 * Both methods internally create a thread. When passing the function pointer and parameters
 * the function is invoked from this internal thread context. So an application should not spend
 * much time here in order not to block the acquisition engine. When using the
 * \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest approach the internal
 * thread pushes it's data into an instance of \b mvIMPACT::acquire::helper::ThreadSafeQueue so
 * capturing can continue if an application does not immediately pick up the data.
 *
 * \note Both ways of using the class have their pros and cons. However the
 * \b mvIMPACT::acquire::helper::ThreadSafeQueue is only served when NOT passing a
 * function pointer.
 *
 * \note Internally an instance to \b mvIMPACT::acquire::FunctionInterface is
 * created. The number of requests available to this \b mvIMPACT::acquire::helper::RequestProvider
 * must (if needed) be configured before starting the acquisition. See \b mvIMPACT::acquire::SystemSettings::requestCount
 * for details.
 *
 * \attention
 * An application shall \b NEVER store the raw pointer (<b>mvIMPACT::acquire::Request</b>*) to
 * requests returned wrapped in <tt>std::shared_ptr</tt> objects! This would jeopardize
 * auto-unlocking of request and thus more or less the purpose of this class. Instead
 * always store the <tt>std::shared_ptr</tt> objects directly. When no longer needed simply
 * dump them. Keeping them all will result in the acquisition engine to run out of buffers.
 * It can only re-use <b>mvIMPACT::acquire::Request</b> when no more <tt>std::shared_ptr</tt>
 * references to it exist. On the other hand make sure you did drop all references to requests before
 * the \b mvIMPACT::acquire::helper::RequestProvider instance that did return them goes out of
 * scope. Not doing this will result in undefined behavior!
 *
 * \if DOXYGEN_CPP_DOCUMENTATION
 * \code
 * // The easiest way to use this class is probably like this:
 * Device* pDev = getDevicePointerFromSomewhere();
 * helper::RequestProvider requestProvider( pDev );
 * requestProvider.acquisitionStart();
 * for(size_t i = 0; i < 5; i++)
 * {
 *   std::shared_ptr<Request> pRequest = requestProvider.waitForNextRequest();
 *   std::cout << "Image captured: " << pRequest->imageOffsetX.read() << "x" << pRequest->imageOffsetY.read() << "@" << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read() << std::endl;
 * }
 * requestProvider.acquisitionStop();
 * \endcode
 * \elseif DOXYGEN_JAVA_DOCUMENTATION
 * \code
 * \endcode
 * \elseif DOXYGEN_PYTHON_DOCUMENTATION
 * \code
 * \endcode
 * \endif
 *
 * a more complex example using the function pointer approach can be found in the description of the corresponding overload of \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStart.
 *
 * \note This class requires a C++11 compliant compiler!
 *
 * \note When using this class together with \b mvIMPACT::acquire::display::ImageDisplay \b CPP_STANDARD_VERSION must be defined to a value greater than or equal to 11! If not
 * some of the magic in \b mvIMPACT::acquire::display::ImageDisplay will not be compiled resulting in the auto-unlocking feature of the requests attached to the display will
 * not work and might result in undefined behavior!
 *
 * \since 2.33.0
 * \ingroup CommonInterface
 */
class RequestProvider final
//-----------------------------------------------------------------------------
{
    struct RequestUnlocker
    {
        std::shared_ptr<FunctionInterface> pFI_;
        explicit RequestUnlocker( std::shared_ptr<FunctionInterface> pFI ) : pFI_( pFI ) {}
        void operator()( Request* pRequest )
        {
            assert( pRequest && "Something weird that needs fixing has happened before: When a custom deleter is attached to a 'Request' objects shared_ptr this request should actually be valid!" );
            pRequest->unlock();
            pFI_->imageRequestSingle();
        }
    };

    Device* pDev_;
    std::shared_ptr<FunctionInterface> pFI_;
    volatile bool boRunCaptureThread_;
    std::unique_ptr<std::thread> pCaptureThread_;
    ThreadSafeQueue<std::shared_ptr<Request> > requestResultQueue_;

    static void captureThreadCallbackInternal( std::shared_ptr<Request> pRequest, RequestProvider* pInstance )
    {
        pInstance->requestResultQueue_.push( pRequest );
    }
    template<typename FUNC, typename ... PARAMS>
    void captureThread( FUNC pFn, PARAMS ...params )
    {
        TDMR_ERROR result = DMR_NO_ERROR;
        while( ( result = static_cast<TDMR_ERROR>( pFI_->imageRequestSingle() ) ) == DMR_NO_ERROR ) {};
        if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
        {
            assert( !"'FunctionInterface.imageRequestSingle' returned with an unexpected result!" );
        }

        manuallyStartAcquisitionIfNeeded( pDev_, *pFI_ );
        const unsigned int timeout_ms = {200};
        while( boRunCaptureThread_ )
        {
            const int requestNr = pFI_->imageRequestWaitFor( timeout_ms );
            if( pFI_->isRequestNrValid( requestNr ) )
            {
                std::shared_ptr<Request> pRequest( pFI_->getRequest( requestNr ), RequestUnlocker( pFI_ ) );
                pFn( pRequest, params... );
            }
        }
        manuallyStopAcquisitionIfNeeded( pDev_, *pFI_ );
        pFI_->imageRequestReset( 0, 0 );
    }
    template<typename FUNC, typename ... PARAMS>
    static void captureThreadStub( RequestProvider* pInstance, FUNC pFn, PARAMS ...params )
    {
        return pInstance->captureThread( pFn, params... );
    }
public:
    /// \brief Creates a new \b mvIMPACT::acquire::helper::RequestProvider instance.
    explicit RequestProvider(
        /// [in] A pointer to a \b mvIMPACT::acquire::Device object obtained from a \b mvIMPACT::acquire::DeviceManager object.
        Device* pDev,
        /// [in] A pointer to a request factory.
        /// By supplying a custom request factory the user can control the type of request objects
        /// that will be created by the function interface.
        RequestFactory* pRequestFactory = nullptr ) : pDev_( pDev ), pFI_( std::make_shared<FunctionInterface>( pDev, pRequestFactory ) ), boRunCaptureThread_( false ) {}
    /// \brief Copy-constructor (<b>deleted</b>)
    /**
     * Objects of this type shall not be copy-constructed!
     */
    RequestProvider( const RequestProvider& src ) = delete;
    /// \brief assignment operator (<b>deleted</b>)
    /**
     * Objects of this type shall not be assigned!
     */
    RequestProvider& operator=( const RequestProvider& rhs ) = delete;
    /// \brief Starts the acquisition.
    /**
     * Will start the acquisition by creating an internal thread. To get access to the data that will be acquired from the device an application passes
     * a callback function and a variable list of arbitrary parameters to this function. Whenever a request becomes ready this function then will be
     * called <b>(from within the internal thread's context!)</b>. What kind of function is passed is completely up to the application. The only restriction
     * for the functions signature is that the first parameter must be <tt>std::shared_ptr<Request> pRequest</tt>.
     *
     * \attention Unlocking will be done automatically when the last reference to the \c std::shared_ptr goes out of
     * scope. As a consequence do \b NOT store the raw pointer to the mvIMPACT::acquire::Request object stored by the \c std::shared_ptr but always
     * the <tt>std::shared_ptr\<Request\></tt> itself in order not to break reference counting. Do not call \b mvIMPACT::acquire::Request::unlock
     * for requests returned wrapped in a <tt>std::shared_ptr\<Request\></tt> (you can but it is not necessary).
     *
     * If the acquisition is already running then calling this function will raise an exception of type \b mvIMPACT::acquire::ImpactAcquireException.
     *
     * \if DOXYGEN_CPP_DOCUMENTATION
     *
     * \code
     * // EXAMPLE
     * //-----------------------------------------------------------------------------
     * struct ThreadParameter
     * //-----------------------------------------------------------------------------
     * {
     *   Device* pDev_;
     *   unsigned int requestsCaptured_;
     *   Statistics statistics_;
     *   explicit ThreadParameter( Device* pDev ) : pDev_( pDev ), requestsCaptured_( 0 ), statistics_( pDev ) {}
     *   ThreadParameter( const ThreadParameter& src ) = delete;
     *   ThreadParameter& operator=( const ThreadParameter& rhs ) = delete;
     * };
     *
     * //-----------------------------------------------------------------------------
     * /// Gets called for each 'Request' object that becomes ready. No need to call 'Request::unlock'!
     * /// Unlocking will be done automatically when the last 'shared_ptr' reference to this
     * /// request goes out of scope.
     * void myThreadCallback( shared_ptr<Request> pRequest, ThreadParameter& threadParameter )
     * //-----------------------------------------------------------------------------
     * {
     *   ++threadParameter.requestsCaptured_;
     *   // display some statistical information every 100th image
     *   if( threadParameter.requestsCaptured_ % 100 == 0 )
     *   {
     *     const Statistics& s = threadParameter.statistics_;
     *     cout << "Info from " << threadParameter.pDev_->serial.read()
     *          << ": " << s.framesPerSecond.name() << ": " << s.framesPerSecond.readS()
     *          << ", " << s.errorCount.name() << ": " << s.errorCount.readS()
     *          << ", " << s.captureTime_s.name() << ": " << s.captureTime_s.readS() << endl;
     *   }
     *   if( pRequest->isOK() )
     *   {
     *     cout << "Image captured: " << pRequest->imageOffsetX.read() << "x" << pRequest->imageOffsetY.read() << "@" << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read() << endl;
     *   }
     *   else
     *   {
     *     cout << "Error: " << pRequest->requestResult.readS() << endl;
     *   }
     * }
     *
     * //-----------------------------------------------------------------------------
     * int main( void )
     * //-----------------------------------------------------------------------------
     * {
     *   DeviceManager devMgr;
     *   Device* pDev = getDeviceFromUserInput( devMgr );
     *   if( pDev == nullptr )
     *   {
     *     cout << "Unable to continue! Press [ENTER] to end the application" << endl;
     *     cin.get();
     *     return 1;
     *  }
     *
     *  cout << "Initialising the device. This might take some time..." << endl;
     *  try
     *  {
     *    pDev->open();
     *  }
     *  catch( const ImpactAcquireException& e )
     *  {
     *    // this e.g. might happen if the same device is already opened in another process...
     *    cout << "An error occurred while opening the device " << pDev->serial.read()
     *         << "(error code: " << e.getErrorCodeAsString() << ").";
     *    return 1;
     *  }
     *
     *  cout << "Press [ENTER] to stop the acquisition thread" << endl;
     *  ThreadParameter threadParam( pDev );
     *  helper::RequestProvider requestProvider( pDev );
     *  requestProvider.acquisitionStart( myThreadCallback, std::ref( threadParam ) );
     *  cin.get();
     *  requestProvider.acquisitionStop();
     *
     *  return 0;
     * }
     * \endcode
     * \elseif DOXYGEN_JAVA_DOCUMENTATION
     * \code
     * \endcode
     * \elseif DOXYGEN_PYTHON_DOCUMENTATION
     * \code
     * \endcode
     * \endif
     *
     * \sa
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStop, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest
     */
    template<typename FUNC, typename ...PARAMS>
    void acquisitionStart(
        /// [in] An arbitrary function that shall be called from the internal thread context whenever a \b mvIMPACT::acquire::Request object becomes ready
        FUNC pFn,
        /// [in] A variable number of arbitrary arguments that shall be passed to the thread callback function passed as the previous argument.
        PARAMS ...params )
    {
        if( boRunCaptureThread_ )
        {
            ExceptionFactory::raiseException( __FUNCTION__, __LINE__, DMR_BUSY, "This object already has its thread running!" );
        }
        boRunCaptureThread_ = true;
        // 'make_unique' was not introduced until C++14
        pCaptureThread_ = std::unique_ptr<std::thread>( new std::thread( &RequestProvider::captureThreadStub<FUNC, PARAMS...>, this, pFn, params... ) );
    }
    /// \brief Starts the acquisition.
    /**
     * Will start the acquisition by creating an internal thread. To get access to captured blocks of data (typically images) an application can periodically call
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest afterwards.
     *
     * If the acquisition is already running then calling this function will raise an exception of type \b mvIMPACT::acquire::ImpactAcquireException.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStop, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest
     */
    void acquisitionStart( void )
    {
        if( boRunCaptureThread_ )
        {
            ExceptionFactory::raiseException( __FUNCTION__, __LINE__, DMR_BUSY, "This object already has its thread running!" );
        }
        boRunCaptureThread_ = true;
        // clean up stuff from previous operations
        requestResultQueue_.clear();
        pFI_->imageRequestReset( 0, 0 );
        // 'make_unique' was not introduced until C++14
        pCaptureThread_ = std::unique_ptr<std::thread>( new std::thread( &RequestProvider::captureThreadStub<void( * )( std::shared_ptr<Request> pRequest, RequestProvider* pInstance ), RequestProvider*>, this, RequestProvider::captureThreadCallbackInternal, this ) );
    }
    /// \brief Stops the acquisition.
    /**
     * Will stop the acquisition previously started by a call to mvIMPACT::acquire::helper::RequestProvider::acquisitionStart.
     *
     * \note
     * If the acquisition is not running then calling this function will do nothing.
     *
     * \note
     * This is a convenience function! Internally this is simply a combination of \b mvIMPACT::acquire::helper::RequestProvider::endThreadExecution and
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForThreadTermination.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStart, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest
     */
    void acquisitionStop( void )
    {
        endThreadExecution();
        waitForThreadTermination();
    }
    /// \brief Signaling the acquisition thread to terminate.
    /**
     * Will stop the acquisition previously started by a call to mvIMPACT::acquire::helper::RequestProvider::acquisitionStart.
     *
     * \note
     * If the acquisition is not running then calling this function will do nothing!
     *
     * To wait for the acquisition thread to terminate
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForThreadTermination has to be called afterwards! To combine
     * \b mvIMPACT::acquire::helper::RequestProvider::endThreadExecution and \b mvIMPACT::acquire::helper::RequestProvider::waitForThreadTermination
     * use \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStop.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStart, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStop, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForThreadTermination, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest
     *
     * \since 2.41.0
     */
    void endThreadExecution( void )
    {
        boRunCaptureThread_ = false;
    }
    /// \brief Waits for the acquisition thread to terminate.
    /**
     *  Will stop the acquisition previously started by a call to mvIMPACT::acquire::helper::RequestProvider::acquisitionStart.
     *
     * In order to make this function return successfully \b mvIMPACT::acquire::helper::RequestProvider::endThreadExecution has to be called before
     * or after(from a different thread then)! To combine \b mvIMPACT::acquire::helper::RequestProvider::endThreadExecution and
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForThreadTermination use \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStop.
     *
     * \note
     * If the acquisition is not running then calling this function will do nothing.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStart, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStop, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::endThreadExecution, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest
     *
     * \since 2.41.0
     */
    void waitForThreadTermination( void )
    {
        if( pCaptureThread_ )
        {
            pCaptureThread_->join();
            pCaptureThread_.reset();
        }
    }
    /// \brief Waits for the next request to become ready and will return a shared_ptr instance to it.
    /**
     * Waits for the next request to become ready and will return a <tt>std::shared_ptr\<Request\></tt> instance to it effectively
     * removing it from the internal queue. This is a blocking function. It will return not unless either a \b mvIMPACT::acquire::Request
     * object became ready, the specified timeout did elapse or \b mvIMPACT::acquire::helper::RequestProvider::terminateWaitForNextRequest has been called from a
     * different thread.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStart, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::terminateWaitForNextRequest
     *
     * \return
     * - true when a new request became ready
     * - false otherwise
     */
    bool waitForNextRequest(
        /// [in] A timeout in milliseconds specifying the maximum time this function shall wait for a request to become ready
        unsigned int ms,
        /// [in,out] A pointer to the storage location that shall receive a pointer to the next \b mvIMPACT::acquire::Request object that becomes ready
        /// or the oldest element already available. An application can pass \a nullptr if a caller just wants
        /// to remove an element from the queue but is not actually interested in it.
        std::shared_ptr<Request>* ppRequest )
    {
        return requestResultQueue_.pop( ms, ppRequest );
    }
    /// \brief Waits for the next request to become ready and will return a shared_ptr instance to it.
    /**
     * Waits for the next request to become ready and will return a <tt>std::shared_ptr\<Request\></tt> instance to it effectively
     * removing it from the internal queue. This is a blocking function. It will return not unless either a mvIMPACT::acquire::Request
     * object became ready, the specified timeout did elapse or \b mvIMPACT::acquire::helper::RequestProvider::terminateWaitForNextRequest has been called from a
     * different thread.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStart, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::terminateWaitForNextRequest
     *
     * \return A <tt>std::shared_ptr\<Request\></tt> either holding a valid pointer to a mvIMPACT::acquire::Request object or \c nullptr if
     * no data became ready and either \b mvIMPACT::acquire::helper::RequestProvider::terminateWaitForNextRequest has been called from another
     * thread or the specified timeout has elapsed.
     */
    std::shared_ptr<Request> waitForNextRequest(
        /// [in] A timeout in milliseconds specifying the maximum time this function shall wait for a request to become ready
        unsigned int ms )
    {
        std::shared_ptr<Request> pRequest;
        requestResultQueue_.pop( ms, &pRequest );
        return pRequest;
    }
    /// \brief Waits for the next request to become ready and will return a shared_ptr instance to it.
    /**
     * Waits for the next request to become ready and will return a <tt>std::shared_ptr\<Request\></tt> instance to it effectively
     * removing it from the internal queue. This is a blocking function. It will return not unless either a mvIMPACT::acquire::Request
     * object became ready or \b mvIMPACT::acquire::helper::RequestProvider::terminateWaitForNextRequest has been called from a
     * different thread.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::RequestProvider::acquisitionStart, \n
     * \b mvIMPACT::acquire::helper::RequestProvider::terminateWaitForNextRequest
     *
     * \return A <tt>std::shared_ptr\<Request\></tt> either holding a valid pointer to a mvIMPACT::acquire::Request object or \c nullptr if
     * no data became ready and \b mvIMPACT::acquire::helper::RequestProvider::terminateWaitForNextRequest has been called from another
     * thread.
     */
    std::shared_ptr<Request> waitForNextRequest( void )
    {
        std::shared_ptr<Request> pRequest;
        requestResultQueue_.pop( &pRequest );
        return pRequest;
    }
    /// \brief Terminates a single wait operation currently pending from another thread without delivering data.
    /**
     * If one or multiple thread(s) are currently executing a \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest operation \b ONE of these
     * threads will get signaled and the call will return.
     *
     * \sa
     * \b mvIMPACT::acquire::helper::RequestProvider::waitForNextRequest
     */
    void terminateWaitForNextRequest( void )
    {
        requestResultQueue_.terminateWait();
    }
};

} // namespace helper
} // namespace acquire
} // namespace mvIMPACT

#ifdef _MSC_VER
#   pragma pop_macro("max")
#endif

#endif //MVIMPACT_ACQUIRE_HELPER_H
