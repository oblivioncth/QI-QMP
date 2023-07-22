// Unit Include
#include "qi-qmp/qmpi.h"

// Qx Includes
#include <qx/core/qx-json.h>

/*! @cond */
namespace Json
{

struct GreetingBody
{
    QJsonObject version;
    QJsonArray capabilities;

    QX_JSON_STRUCT(version, capabilities);
};

struct Greeting
{
    GreetingBody QMP;

    QX_JSON_STRUCT(QMP);
};

struct SuccessResponse
{
    static inline const QString IDENTIFIER_KEY =u"return"_s;
};

struct ErrorBody
{
    QString eClass;
    QString desc;

    QX_JSON_STRUCT_X(
        QX_JSON_MEMBER_ALIASED(eClass, "class"),
        QX_JSON_MEMBER(desc)
    );
};

struct ErrorResponse
{
    static inline const QString IDENTIFIER_KEY =u"error"_s;

    ErrorBody error;

    QX_JSON_STRUCT(error);
};

struct Timestamp
{
    double seconds;
    double microseconds;

    QX_JSON_STRUCT(seconds, microseconds);
};

struct AsyncEvent
{
    static inline const QString IDENTIFIER_KEY =u"event"_s;

    QString event;
    QJsonObject data;
    Timestamp timestamp;

    QX_JSON_STRUCT(event, data, timestamp);
};

// TODO: Use QX_JSON for serializing these after the serialization portion of QX_JSON is implemented
struct Execute
{
    static inline const QString IDENTIFIER_KEY =u"execute"_s;
    static inline const QString ARGUMENTS =u"arguments"_s;
};

static inline const QString NEGOTIATION_COMMAND =u"qmp_capabilities"_s;
}
/*! @endcond */

//===============================================================================================================
// Qmpi
//===============================================================================================================

/*!
 *  @class Qmpi qi-qmp/qmpi.h
 *
 *  @brief The Qmpi class is an interface through which to communicate with a QEMU instance via the
 *  QEMU Machine Protocol
 *
 *  The interface works asynchronously, providing updates via signals and requires an event loop to function.
 *
 *  All commands issued through the interface may optionally contain a context parameter in the form of
 *  std::any, which will be provided along with that command's corresponding response when
 *  responseReceived() or error errorResponseReceived() is emitted.
 *
 *  @note Currently only connecting to servers listening for TCP connections is supported (i.e. no local/UNIX
 *  socket connections).
 *
 *  @sa QTcpSocket.
 */

//-Class Enums-----------------------------------------------------------------------------------------------
//Public:
/*!
 *  @enum Qmpi::State
 *
 *  This enum describes the different states in which the interface can be.
 */

/*!
 *  @var Qmpi::State Qmpi::Disconnected
 *  The interface is not connected to any server and is inactive.
 */

/*!
 *  @var Qmpi::State Qmpi::Connecting
 *  The interface is attempting to connect to its assigned server.
 */

/*!
 *  @var Qmpi::State Qmpi::AwaitingWelcome
 *  The interface is waiting to receive the server's greeting.
 */

/*!
 *  @var Qmpi::State Qmpi::Negotiating
 *  The interface is automatically handling capabilities negotiation.
 */

/*!
 *  @var Qmpi::State Qmpi::Idle
 *  The interface is connected to a server and is not sending any commands nor expecting any responses.
 */

/*!
 *  @var Qmpi::State Qmpi::Closing
 *  The interface is handling disconnection from the server.
 */

/*!
 *  @var Qmpi::State Qmpi::SendingCommand
 *  The interface is encoding/ending a command to the server.
 */

/*!
 *  @var Qmpi::State Qmpi::AwaitingMessage
 *  The interface is waiting for a response from the server.
 */

/*!
 *  @var Qmpi::State Qmpi::ReadingMessage
 *  The interface is parsing a response from the server.
 */

/*!
 *  @enum Qmpi::CommunicationError
 *
 *  This enum describes the various communication errors that can occur.
 */

/*!
 *  @var Qmpi::CommunicationError Qmpi::WriteFailed
 *  Writing data to the server failed.
 */

/*!
 *  @var Qmpi::CommunicationError Qmpi::ReadFailed
 *  Reading data from the server failed.
 */

/*!
 *  @var Qmpi::CommunicationError Qmpi::TransactionTimeout
 *  The server ran out of time to respond.
 */

/*!
 *  @var Qmpi::CommunicationError Qmpi::UnexpectedReceive
 *  The interface received a response when it was not expecting one.
 */

/*!
 *  @var Qmpi::CommunicationError Qmpi::UnexpectedResponse
 *  The interface received a response that does not follow the known protocol.
 */

//-Constructor-------------------------------------------------------------
//Public:
/*!
 *  Constructs a QMP interface with parent @a parent.
 *
 *  The connection address is set to QHostAddress::LocalHost and the port is set to 4444.
 *
 *  @sa setAddress() and setPort().
 */
Qmpi::Qmpi(QObject* parent) :
    QObject(parent),
    mHostId(QHostAddress::LocalHost),
    mPort(4444),
    mSocket(this),
    mState(Disconnected)
{
    // Other init
    mTransactionTimer.setSingleShot(true);
    mTransactionTimer.setInterval(30000);

    // Connections - Local Handling
    connect(&mSocket, &QTcpSocket::stateChanged, this, &Qmpi::handleSocketStateChange);
    connect(&mSocket, &QTcpSocket::readyRead, this, &Qmpi::handleReceivedData);
    connect(&mTransactionTimer, &QTimer::timeout, this, &Qmpi::handleTransactionTimeout);

    // Connections - Straight Forwards
    connect(&mSocket, &QTcpSocket::errorOccurred, this, &Qmpi::connectionErrorOccurred);
    connect(&mSocket, &QTcpSocket::disconnected, this, &Qmpi::disconnected);
}

/*!
 *  Constructs a QMP interface configured to connect to a QEMU instance at address @a address on port
 *  @a port. @a port is specified in native byte order.
 *
 *  The parent of the object is set to @a parent.
 */
Qmpi::Qmpi(const QHostAddress& address, quint16 port, QObject* parent) :
      Qmpi(parent)
{
    mPort = port;
    mHostId = address;
}

/*!
 *  Constructs a QMP interface configured to connect to a QEMU instance at hostname @a hostname on port
 *  @a port. @a hostname may be an IP address in string form (e.g., "192.168.1.1", or
 *  "7e72:692b:9797:8310:8e68:3a98:3a21:473c"), or it may be a host name (e.g., "example.com"). The
 *  interface will do a lookup only if required. @a port is specified in native byte order.
 *
 *  The parent of the object is set to @a parent.
 */
Qmpi::Qmpi(const QString& hostname, quint16 port, QObject* parent) :
    Qmpi(parent)
{
    mPort = port;
    mHostId = hostname;
}

//-Destructor-------------------------------------------------------------
//Public:
/*!
 *  Destroys the interface, closing the underlying connection immediately if necessary.
 */
Qmpi::~Qmpi() {}

//-Instance Functions---------------------------------------------------------------
//Private:
void Qmpi::changeState(State newState)
{
    if(newState != mState)
    {
        mState = newState;
        emit stateChanged(mState);
    }
}

bool Qmpi::startTransactionTimer()
{
    /* Treat negative values as disabling the timeout. This must be done explicitly as starting
     * a QTimer with a negative interval causes a warning to be logged stating the interval
     * cannot be negative, even though it seems to work fine
     *
     * Also don't restart the timer if it's already running
     */
    if(mTransactionTimer.interval() >= 0 && !mTransactionTimer.isActive())
    {
        mTransactionTimer.start();
        return true;
    }
    else
        return false;

}
bool Qmpi::stopTransactionTimer()
{
    // Used to enforce symmetry for both starting and stopping the timer (i.e. not interacting with it directly)
    if(mTransactionTimer.isActive())
    {
        mTransactionTimer.stop();
        return true;
    }
    else
        return false;

}

void Qmpi::reset()
{
    mExecutionQueue = {};
    stopTransactionTimer();
}

void Qmpi::raiseCommunicationError(CommunicationError error)
{
    emit communicationErrorOccurred(error);
    mSocket.abort();
}

void Qmpi::finish()
{
    reset();
    emit finished();
}

void Qmpi::negotiateCapabilities()
{
    changeState(State::Negotiating);
    if(!sendCommand(Json::NEGOTIATION_COMMAND))
        return;
    startTransactionTimer();
}

bool Qmpi::sendCommand(QString command, QJsonObject args)
{
    // Build JSON object
    QJsonObject commandObject;
    commandObject[Json::Execute::IDENTIFIER_KEY] = command;
    if(!args.isEmpty())
        commandObject[Json::Execute::ARGUMENTS] = args;

    // Convert to raw message
    QJsonDocument execution(commandObject);
    QByteArray message = execution.toJson(QJsonDocument::Compact); // Is UTF-8

    // Send message (account for rare case of a full buffer or other write error)
    while(!message.isEmpty())
    {
        // Write as much as possible
        qint64 bytesWritten = mSocket.write(message);

        // Check for error
        if(bytesWritten == -1)
        {
            raiseCommunicationError(CommunicationError::WriteFailed);
            return false;
        }

        // Account for bytes written
        message = message.sliced(bytesWritten, message.size() - bytesWritten);
    }

    // Return success
    return true;
}

void Qmpi::propagate()
{
    // Drive Transactions
    if(!mExecutionQueue.empty())
    {
        // Send next command
        changeState(State::SendingCommand);
        ExecutionTask& task = mExecutionQueue.front();
        sendCommand(task.command, task.args);

        // Set wait state
        changeState(State::AwaitingMessage);

        // Start timeout timer
        startTransactionTimer();
    }
    else
    {
        emit commandQueueExhausted();
        changeState(State::Idle);
    }
}

void Qmpi::processServerMessage(const QJsonObject& jMsg)
{
    // Stop transaction timer since message has arrived
    bool timerWasRunning = stopTransactionTimer();

    if(mState == State::AwaitingWelcome)
    {
        // Parse the greeting
        if(!processGreetingMessage(jMsg))
            return;

        // Automatically proceed to enabling commands since this currently doesn't support extra capabilities
        negotiateCapabilities();
    }
    else if(mState == State::Negotiating)
    {
        // Just ensure an error didn't happen, value doesn't matter (should be empty)
        if(!jMsg.contains(Json::SuccessResponse::IDENTIFIER_KEY))
        {
            raiseCommunicationError(CommunicationError::UnexpectedResponse);
            return;
        }

        // Proceed to command entry phase
        changeState(State::Idle);
        emit readyForCommands();
    }
    else // Command responses and events
    {
        changeState(State::ReadingMessage);

        if(jMsg.contains(Json::AsyncEvent::IDENTIFIER_KEY))
        {
            if(!processEventMessage(jMsg))
                return;

            // Restart the transaction timer if it was running since events are just informative
            // and don't control flow
            if(timerWasRunning)
                startTransactionTimer();
        }
        else
        {
            // Error if not expecting a response
            if(mExecutionQueue.empty())
            {
                raiseCommunicationError(CommunicationError::UnexpectedReceive);
                return;
            }

            // Check for each response type
            if(jMsg.contains(Json::SuccessResponse::IDENTIFIER_KEY))
            {
                if(!processSuccessMessage(jMsg))
                    return;

                // Send next command
                propagate();
            }
            else if(jMsg.contains(Json::ErrorResponse::IDENTIFIER_KEY))
            {
                if(!processErrorMessage(jMsg))
                    return;

                // Send next command
                propagate();
            }
            else
            {
                raiseCommunicationError(CommunicationError::UnexpectedResponse);
                return;
            }
        }
    }
}

bool Qmpi::processGreetingMessage(const QJsonObject& jGreeting)
{
    Json::Greeting greeting;
    if(Qx::parseJson(greeting, jGreeting).isValid())
    {
        raiseCommunicationError(CommunicationError::UnexpectedResponse);
        return false;
    }

    Json::GreetingBody gb = greeting.QMP;

    emit connected(gb.version, gb.capabilities);
    return true;
}

bool Qmpi::processSuccessMessage(const QJsonObject& jSuccess)
{
    Q_ASSERT(!mExecutionQueue.empty());

    // Get value
    QJsonValue value = jSuccess[Json::SuccessResponse::IDENTIFIER_KEY];
    std::any context = mExecutionQueue.front().context;
    mExecutionQueue.pop();
    emit responseReceived(value, context);

    return true; // Can't fail as of yet
}

bool Qmpi::processErrorMessage(const QJsonObject& jError)
{
    Q_ASSERT(!mExecutionQueue.empty());

    std::any context = mExecutionQueue.front().context;
    mExecutionQueue.pop();

    Json::ErrorResponse er;
    if(Qx::parseJson(er, jError).isValid())
    {
        raiseCommunicationError(CommunicationError::UnexpectedResponse);
        return false;
    }

    Json::ErrorBody eb = er.error;

    emit errorResponseReceived(eb.eClass, eb.desc, context);
    return true;
}

bool Qmpi::processEventMessage(const QJsonObject& jEvent)
{
    Json::AsyncEvent ae;
    if(Qx::parseJson(ae, jEvent).isValid())
    {
        raiseCommunicationError(CommunicationError::UnexpectedResponse);
        return false;
    }

    // Convert native timestamp to QDateTime
    Json::Timestamp ts = ae.timestamp;

    qint64 milliseconds = -1;
    if(ts.seconds != -1)
    {
        milliseconds = std::round(ts.seconds) * 1000;
        if(ts.microseconds != -1)
            milliseconds += std::round(ts.microseconds / 1000);
    }
    QDateTime standardTimestamp = (milliseconds != -1) ? QDateTime::fromMSecsSinceEpoch(milliseconds) : QDateTime();

    // Notify
    emit eventReceived(ae.event, ae.data, standardTimestamp);

    // Return success
    return true;
}

//Public:
/*!
 *  Returns the state of the interface.
 */
Qmpi::State Qmpi::state() const { return mState; }

/*!
 *  Returns the IP address the interface is configured to connect to if it was set as a
 *  QHostAddress; otherwise, returns QHostAddress::Null.
 *
 *  @sa setAddress() and port().
 */
QHostAddress Qmpi::address() const
{
    const QHostAddress* ha = std::get_if<QHostAddress>(&mHostId);
    return ha ? *ha : QHostAddress::Null;
}

/*!
 *  Returns the hostname the interface is configured to connect to if it was set as a
 *  hostname; otherwise, returns a null string.
 *
 *  @sa setHostname() and port().
 */
QString Qmpi::hostname() const
{
    const QString* hn = std::get_if<QString>(&mHostId);
    return hn ? *hn : QString();
}

/*!
 *  Returns the port the interface is configured to connect through.
 *
 *  @sa setPort() and address().
 */
quint16 Qmpi::port() const { return mPort; }

/*!
 *  Returns the transaction timeout of the interface.
 *
 *  The default is 30,000 milliseconds.
 *
 *  @sa setTransactionTimeout().
 */
int Qmpi::transactionTimeout() const { return mTransactionTimer.interval(); }

/*!
 *  Sets the address of the interface to @a address.
 *
 *  This function does nothing if the connection is currently active
 *
 *  @sa address() and setHostname().
 */
void Qmpi::setAddress(const QHostAddress address)
{
    if(!isConnectionActive())
        mHostId = address;
}

/*!
 *  Sets the hostname of the interface to @a hostname.
 *
 *  This function does nothing if the connection is currently active
 *
 *  @sa hostname() and setAddress().
 */
void Qmpi::setHostname(const QString hostname)
{
    if(!isConnectionActive())
        mHostId = hostname;
}

/*!
 *  Sets the port, specified in native byte order, of the interface to @a port.
 *
 *  This function does nothing if the connection is currently active
 *
 *  @sa port() and setAddress().
 */
void Qmpi::setPort(quint16 port)
{
    if(!isConnectionActive())
        mPort = port;
}

/*!
 *  Sets the transaction timeout of the interface to @a timeout.
 *
 *  The transaction timeout is how long the connected QEMU instance has to reply with a
 *  complete message after sending a command before the connection is automatically closed.
 *
 *  The received message does not have to be a response to that particular command; any
 *  received message will reset the timeout, while sending additional commands will not.
 *
 *  If a transaction timeout occurs communicationErrorOccurred() will be emitted with the
 *  value @ref TransactionTimeout. If all sent commands have been processed and the
 *  interface is not currently expecting any responses the timeout will not be in effect.
 *
 *  Setting this value to @c -1 will disable the timeout.
 *
 *  @sa transactionTimeout().
 */
void Qmpi::setTransactionTimeout(int timeout) { mTransactionTimer.setInterval(timeout); }

/*!
 *  Attempts to make a connection to the QEMU instance via the interfaces configured
 *  address and port.
 *
 *  The interface's state immediately changes to @ref Connecting, followed by
 *  @ref AwaitingWelcome if the underlying socket successfully establishes a connection.
 *  Once the interface has received the server's welcome message, connected() is emitted
 *  and Qmpi enters the @ref Negotiating state. At this point capabilities negotiation is
 *  handled automatically since only the base protocol capabilities are currently supported,
 *  upon which readyForCommands() is emitted and the interface enters the @ref Idle state.
 *
 *  At any point, the interface can emit connectionErrorOccurred() or communicationErrorOccurred()
 *  to signal an error occurred.
 *
 *  @sa state(), and disconnectFromHost().
 */
void Qmpi::connectToHost()
{
    // Bail if already working
    if(isConnectionActive())
        return;

    // Connect
    changeState(State::Connecting);

    if(std::holds_alternative<QHostAddress>(mHostId))
        mSocket.connectToHost(std::get<QHostAddress>(mHostId), mPort);
    else if(std::holds_alternative<QString>(mHostId))
        mSocket.connectToHost(std::get<QString>(mHostId), mPort);
    else
        qFatal("Unhandled host id variant");
}

/*!
 *  Initiates closure of the interface's connection and enters the @ref Closing state. If there is
 *  pending data waiting to be written, Qmpi waits until all data has been written before the connection
 *  is closed. Eventually, it will enter the @ref Disconnected state and emit the disconnected() signal.
 *
 *  @sa connectToHost(), and abort().
 */
void Qmpi::disconnectFromHost()
{
    // Bail if already disconnected
    if(!isConnectionActive())
        return;

    // Disconnect
    changeState(State::Closing);
    mSocket.disconnectFromHost();
}

/*!
 *  Immediately closes the interface's connection, discarding any pending data.
 */
void Qmpi::abort()
{
    // TODO: May want to have this emit an 'abort' error since QAbstractSocket doesn't do that.
    // It would have to be part of CommunicationError

    // Bail if already disconnected
    if(!isConnectionActive())
        return;

    // Force disconnect
    // Shouldn't change state manually as it should go to Disconnected almost instantly,
    // and this way a emission of stateChanged() is avoided.
    mSocket.abort();
}

/*!
 *  Returns @c true if the interface's connection is active is any way. That is, the interface is in any
 *  state other than @ref Disconnected; otherwise, returns @c false.
 *
 *  @sa isConnected().
 */
bool Qmpi::isConnectionActive() const { return mState != State::Disconnected; }

/*!
 *  Returns @c true if the interface has fully connected to the QEMU instance, meaning the underlying
 *  socket successfully established a connection and the server's greeting was received.
 *
 *  @sa isConnectionActive().
 */
bool Qmpi::isConnected() const
{
    static QSet<State> notFullyConnectedStates = {
        Disconnected,
        Connecting,
        AwaitingWelcome
    };

    return !notFullyConnectedStates.contains(mState);
}

/*!
 *  Executes the provided command on the connected QEMU instance.
 *
 *  @param command The name of the command to execute.
 *  @param args The command's arguments, if any.
 *  @param context An optional object that will be provided with server's response to the command.
 *
 *  The command is placed into an internal queue to be sent when it reaches the front. Only one command
 *  is sent at a time, with the interface waiting for a response before sending the next.
 *
 *  The server responds to commands in the same order they are received.
 *
 *  The @a context parameter is useful for identifying which command a response is directed towards
 *  or associating the response to a command with specific data or actions (i.e. a callback function).
 *
 *  @note readyForCommands() must have been emitted before commands can be sent.
 */
void Qmpi::execute(QString command, QJsonObject args, std::any context)
{
    if(!isConnected() || mState == State::Negotiating)
        return;

    // Add command to queue
    mExecutionQueue.push({
        .command = command,
        .args = args,
        .context = context
    });

    // Ensure commands are being processed
    if(mState == Idle)
        propagate();
}

//-Signals & Slots------------------------------------------------------------------------------------------------------------
//Private Slots:
void Qmpi::handleSocketStateChange(QAbstractSocket::SocketState socketState)
{
    /* Provides finer awareness of socket state
     * i.e. disconnected isn't emitted if connection fails so if only using that signal cleanup may also
     * need to be prompted from the slot connected to errorOccurred by explicitly checking for a failed connection
     * there. But this way we can just check for a change back to the Unconnected state.
     */
    switch(socketState)
    {
        case QAbstractSocket::SocketState::ConnectedState:
            changeState(State::AwaitingWelcome);
            startTransactionTimer(); // Enforce timeout for welcome message
            break;
        case QAbstractSocket::SocketState::UnconnectedState:
            changeState(State::Disconnected);
            /* NOTE: The implementation of QAbstractSocket inconstantly fires related stateChanged() and
             * errorOccurred() signals, so sometimes an error that caused the socket to change to the
             * disconnected state will have its corresponding signal arrive after the state change one,
             * meaning that seeing stateChanged(SocketState::UnconnectedState) does not necessarily mean
             * the socket is 100% finished and has emitted everything else. To get around this, a
             * single shot timer is used to queue `finished()` so that the current routine within
             * QAbstractSocket that emitted stateChanged() can finish emitting anything else (since
             * they will be via direct connections to here) before the interface emits finished.
             *
             * This should guarantee that finish() is only emitted after the socket is truly "done", though
             * potentially if QAbstractSocket uses internal queued connections and the signals it fires
             * after the one that triggered this slot are queued, they would be processed after finished()
             * here since it will get queued first. Realistically that shouldn't be the case however. In the
             * event this comes to pass, there is no choice but to use the wonky approach that was reverted
             * via bfdef954005809a6de527e6dc05b74d1ec51e11f
             */
            QTimer::singleShot(0, this, &Qmpi::finish);
            break;
        default:
            break;
    }
}

void Qmpi::handleReceivedData()
{
    // Handle each full message that's available
    while(mSocket.canReadLine())
    {
        QByteArray rawMessage = mSocket.readLine();
        if(rawMessage.isEmpty()) // Should be unnecessary because of while condition, but meh
        {
            raiseCommunicationError(CommunicationError::ReadFailed);
            return;
        }

        QJsonDocument interpretedMessage = QJsonDocument::fromJson(rawMessage);
        if(interpretedMessage.isNull() || !interpretedMessage.isObject()) // Message root should always be an object
        {
            raiseCommunicationError(CommunicationError::UnexpectedResponse);
            return;
        }

        processServerMessage(interpretedMessage.object());
    }
}

void Qmpi::handleTransactionTimeout() { raiseCommunicationError(CommunicationError::TransactionTimeout); }

//-Signals------------------------------------------------------------------------------------------------
/*!
 *  @fn void Qmpi::connected(QJsonObject version, QJsonArray capabilities)
 *
 *  This signal is emitted once the interface has finished connecting to the QEMU instance and performing
 *  capabilities negotiation.
 *
 *  @a version contains the server's version information, while @a capabilities describes the server's
 *  QMP capabilities.
 *
 *  @sa disconnected(), readyForCommands().
 */

/*!
 *  @fn void Qmpi::readyForCommands()
 *
 *  This signal is emitted once capabilities negotiation has been completed and the server is ready to
 *  receive commands.
 *
 *  @sa connected().
 */

/*!
 *  @fn void Qmpi::disconnected()
 *
 *  This signal is emitted when the interface has fully disconnected from the QEMU instance.
 *
 *  @note This signal will only be emitted if the interface managed to fully connect to the server.
 *
 *  @warning If you need to delete the sender() of this signal in a slot connected to it, use the
 *  deleteLater() function.
 *
 *  @sa connected(), finished(), isConnectionActive().
 */

/*!
 *  @fn void Qmpi::finished()
 *
 *  This signal is emitted when the interface returns to the @ref Disconnected state for any reason,
 *  regardless of whether or not a complete connection to the server was ever achieved.
 *
 *  @warning If you need to delete the sender() of this signal in a slot connected to it, use the
 *  deleteLater() function.
 *
 *  @sa disconnected(), stateChanged().
 */

/*!
 *  @fn void Qmpi::responseReceived(QJsonValue value, std::any context)
 *
 *  This signal is emitted when a success (i.e. `return`) response is received from the server
 *  after executing a command.
 *
 *  @a value contains the return value of the command, which will be an empty QJsonObject if the command
 *  does not return data, while @a context contains the context object provided when the command was
 *  executed, if set.
 *
 *  @sa errorResponseReceived().
 */

/*!
 *  @fn void Qmpi::eventReceived(QString name, QJsonObject data, QDateTime timestamp)
 *
 *  This signal is emitted when an asynchronous event is posted by the server.
 *
 *  The signal parameters provide the @a name of the event, the event's @a data and its @a timestamp.
 *
 *  @note QEMU instances record the time of events with microsecond precision, while QDateTime is only
 *  capable of millisecond precision; therefore, the timestamp of all events are rounded to the
 *  nearest millisecond.
 *
 *  @sa responseReceived().
 */

/*!
 *  @fn void Qmpi::connectionErrorOccurred(QAbstractSocket::SocketError error)
 *
 *  This signal is emitted when the underlying socket experiences a connection error, with @a error
 *  containing the type of error.
 *
 *  @note When such and error occurs the connection is immediately aborted, all pending reads/writes are discarded,
 *  and the interface enters the @ref Disconnected state.
 */

/*!
 *  @fn void Qmpi::communicationErrorOccurred(Qmpi::CommunicationError error)
 *
 *  This signal is emitted when IO with the server fails, the known QMP protocol is violated during communication,
 *  or the server otherwise behaves unexpectedly. @a error contains the type of communication error.
 *
 *  @note When such and error occurs the connection is immediately aborted, all pending reads/writes are discarded,
 *  and the interface enters the @ref Disconnected state.
 */

/*!
 *  @fn void Qmpi::errorResponseReceived(QString errorClass, QString description, std::any context)
 *
 *  This signal is emitted when the an error response is received from the server after executing
 *  a command.
 *
 *  @a errorClass contains the type of error and @a description contains a human readable
 *  summary of the error, while @a context contains the context object provided when the
 *  command was executed, if set.
 *
 *  @sa responseReceived().
 */

/*!
 *  @fn void Qmpi::stateChanged(Qmpi::State state)
 *
 *  This signal is emitted when the interface's state changes, with @a state containing the new state.
 *
 *  @sa connected(), disconnected(), readyForCommands() and finished().
 */

/*!
 *  @fn void Qmpi::commandQueueExhausted()
 *
 *  This signal is emitted when the interface's command queue becomes empty and the response to the last command
 *  in the queue has been received, just before it enters State::Idle.
 *
 *  @sa stateChanged(), and execute().
 */
