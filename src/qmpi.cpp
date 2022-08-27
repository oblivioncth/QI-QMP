// Unit Include
#include "qi-qmp/qmpi.h"

// Qx Includes
#include <qx/core/qx-json.h>
#include <qx/core/qx-setonce.h>

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
//Private:
Qmpi::Qmpi(quint16 port, QObject* parent) :
    QObject(parent),
    mPort(port),
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
    connect(&mSocket, &QTcpSocket::errorOccurred, this, &Qmpi::connectionErrorOccured);
    connect(&mSocket, &QTcpSocket::disconnected, this, &Qmpi::disconnected);
}
//Public:
/*!
 *  Constructs a QMP interface configured to connect to a QEMU instance at address @a address on port
 *  @a port. @a port is specified in native byte order.
 *
 *  The parent of the object is set to @a parent.
 */
Qmpi::Qmpi(const QHostAddress& address, quint16 port, QObject* parent) :
      Qmpi(port, parent)
{
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
    Qmpi(port, parent)
{
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
    emit communicationErrorOccured(error);
    mSocket.abort();
}

void Qmpi::negotiateCapabilities()
{
    changeState(State::Negotiating);
    if(!sendCommand(NEGOTIATION_COMMAND))
        return;
    startTransactionTimer();
}

bool Qmpi::sendCommand(QString command, QJsonObject args)
{
    // Build JSON object
    QJsonObject commandObject;
    commandObject[JsonKeys::EXECUTE] = command;
    if(!args.isEmpty())
        commandObject[JsonKeys::Execute::ARGUMENTS] = args;

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
            raiseCommunicationError(CommunicationError::ReadFailed);
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
        changeState(State::Idle);
}

void Qmpi::processServerMessage(const QJsonObject& msg)
{
    // Stop transaction timer since message has arrived
    bool timerWasRunning = stopTransactionTimer();

    if(mState == State::AwaitingWelcome)
    {
        // Get and parse the greeting
        QJsonObject greeting;
        if(Qx::Json::checkedKeyRetrieval(greeting, msg, JsonKeys::GREETING).isValid())
        {
            raiseCommunicationError(CommunicationError::UnexpectedResponse);
            return;
        }

        if(!processGreetingMessage(greeting))
            return;

        // Automatically proceed to enabling commands since this currently doesn't support extra capabilities
        negotiateCapabilities();
    }
    else if(mState == State::Negotiating)
    {
        // Just ensure an error didn't happen, value doesn't matter (should be empty)
        if(!msg.contains(JsonKeys::RETURN))
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

        if(msg.contains(JsonKeys::EVENT))
        {
            if(!processEventMessage(msg))
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
            if(msg.contains(JsonKeys::RETURN))
            {
                if(!processReturnMessage(msg))
                    return;

                // Send next command
                propagate();
            }
            else if(msg.contains(JsonKeys::ERROR))
            {
                // Get and parse the error
                QJsonObject error;
                if(Qx::Json::checkedKeyRetrieval(error, msg, JsonKeys::ERROR).isValid())
                {
                    raiseCommunicationError(CommunicationError::UnexpectedResponse);
                    return;
                }

                if(!processErrorMessage(error))
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

bool Qmpi::processGreetingMessage(const QJsonObject& greeting)
{
    QJsonObject version;
    QJsonArray capabilities;
    if(Qx::Json::checkedKeyRetrieval(version, greeting, JsonKeys::Greeting::VERSION).isValid())
    {
        raiseCommunicationError(CommunicationError::UnexpectedResponse);
        return false;
    }
    if(Qx::Json::checkedKeyRetrieval(capabilities, greeting, JsonKeys::Greeting::CAPABILITIES).isValid())
    {
        raiseCommunicationError(CommunicationError::UnexpectedResponse);
        return false;
    }

    emit connected(version, capabilities);
    return true;
}

bool Qmpi::processReturnMessage(const QJsonObject& ret)
{
    Q_ASSERT(!mExecutionQueue.empty());

    QJsonValue value = ret[JsonKeys::RETURN];
    std::any context = mExecutionQueue.front().context;
    mExecutionQueue.pop();
    emit responseReceived(value, context);

    return true; // Can't fail as of yet
}

bool Qmpi::processErrorMessage(const QJsonObject& error)
{
    Q_ASSERT(!mExecutionQueue.empty());

    std::any context = mExecutionQueue.front().context;
    mExecutionQueue.pop();

    QString errorClass;
    QString description;
    if(Qx::Json::checkedKeyRetrieval(errorClass, error, JsonKeys::Error::CLASS).isValid())
    {
        raiseCommunicationError(CommunicationError::UnexpectedResponse);
        return false;
    }
    if(Qx::Json::checkedKeyRetrieval(description, error, JsonKeys::Error::DESCRIPTION).isValid())
    {
        raiseCommunicationError(CommunicationError::UnexpectedResponse);
        return false;
    }

    emit errorResponseReceived(errorClass, description, context);
    return true;
}

bool Qmpi::processEventMessage(const QJsonObject& event)
{
    QString eventName;
    QJsonObject data;
    QJsonObject timestamp;
    double seconds;
    double microseconds;

    // Get all the values (check for error after since there's a decent number)
    Qx::SetOnce<bool> jsonError(false);

    jsonError = Qx::Json::checkedKeyRetrieval(eventName, event, JsonKeys::EVENT).isValid();
    jsonError = Qx::Json::checkedKeyRetrieval(data, event, JsonKeys::Event::DATA).isValid();
    jsonError = Qx::Json::checkedKeyRetrieval(seconds, timestamp, JsonKeys::Event::Timestamp::SECONDS).isValid();
    jsonError = Qx::Json::checkedKeyRetrieval(microseconds, timestamp, JsonKeys::Event::Timestamp::MICROSECONDS).isValid();

    // Check if there was an error at any point
    if(jsonError.value())
    {
        raiseCommunicationError(CommunicationError::UnexpectedResponse);
        return false;
    }

    // Convert native timestamp to QDateTime
    qint64 milliseconds = -1;
    if(seconds != -1)
    {
        milliseconds = std::round(seconds) * 1000;
        if(microseconds != -1)
            milliseconds += std::round(microseconds / 1000);
    }
    QDateTime standardTimestamp = (milliseconds != -1) ? QDateTime::fromMSecsSinceEpoch(milliseconds) : QDateTime();

    // Notify
    emit eventReceived(eventName, data, standardTimestamp);

    // Return success
    return true;
}

//Public:
/*!
 *  Returns the IP address the interface is configured to connect to if it was constructed using a
 *  QHostAddress; otherwise, returns QHostAddress::Null.
 *
 *  @sa port().
 */
QHostAddress Qmpi::address() const
{
    const QHostAddress* ha = std::get_if<QHostAddress>(&mHostId);
    return ha ? *ha : QHostAddress::Null;
}

/*!
 *  Returns the hostname the interface is configured to connect to if it was constructed using a
 *  hostname; otherwise, returns a null string.
 *
 *  @sa port().
 */
QString Qmpi::hostname() const
{
    const QString* hn = std::get_if<QString>(&mHostId);
    return hn ? *hn : QString();
}

/*!
 *  Returns the port the interface is configured to connect through.
 *
 *  @sa address().
 */
quint16 Qmpi::port() const { return mPort; }

/*!
 *  Returns the state of the interface.
 */
Qmpi::State Qmpi::state() const { return mState; }

/*!
 *  Sets the transaction timeout of the interface to @a timeout.
 *
 *  The transaction timeout is how long the connected QEMU instance has to reply with a
 *  complete message after sending a command before the connection is automatically closed.
 *
 *  The received message does not have to be a response to that particular command; any
 *  received message will reset the timeout, while sending additional commands will not.
 *
 *  If a transaction timeout occurs communicationErrorOccured() will be emitted with the
 *  value @ref TransactionTimeout. If all sent commands have been processed and the
 *  interface is not currently expecting any responses the timeout will not be in effect.
 *
 *  Setting this value to @c -1 will disable the timeout.
 *
 *  @sa transactionTimeout().
 */
void Qmpi::setTransactionTimeout(int timeout) { mTransactionTimer.setInterval(timeout); }

/*!
 *  Returns the transaction timeout of the interface.
 *
 *  The default is 30,000 milliseconds.
 *
 *  @sa setTransactionTimeout().
 */
int Qmpi::transactionTimeout() const { return mTransactionTimer.interval(); }

/*!
 *  Attempts to make a connection to the QEMU instance specified by IP address and port
 *  during the interface's construction.
 *
 *  The interface's state immediately changes to @ref Connecting, followed by
 *  @ref AwaitingWelcome if the underlying socket successfully establishes a connection.
 *  Once the interface has received the server's welcome message connected() is emitted
 *  and Qmpi enters the @ref Negotiating state. At this point capabilities negotiation is
 *  handled automatically since only the base protocol capabilities are currently supported,
 *  upon which readyForCommands() is emitted and the interface enters the @ref Idle state.
 *
 *  At any point, the interface can emit connectionErrorOccurred() or communicationErrorOccurred()
 *  signal an error occurred.
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
        throw std::runtime_error(std::string(Q_FUNC_INFO) + " unhandled host id variant");
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
    // Bail if already disconnected
    if(!isConnectionActive())
        return;

    // Force disconnect
    // Shouldn't change state manually as it should go to Disconnected almost instantly,
    // and this way a emission of stateChanged() is avoided.
    mSocket.abort();
}

/*!
 *  Returns @c true if the interfaces connection is active is any way. That is, the interface is in any
 *  state other than @ref Disconnected; otherwise, returns @c false.
 *
 *  @sa isConnected().
 */
bool Qmpi::isConnectionActive() const { return mState != State::Disconnected; }

/*!
 *  Returns @c true if the interfaces has fully connected to the QEMU instance, meaning the underlying
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
            reset();
            emit finished();
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
 *  received commands.
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
 *  regardless of whether or a complete connection to the server was ever achieved.
 *
 *  @warning If you need to delete the sender() of this signal in a slot connected to it, use the
 *  deleteLater() function.
 *
 *  @sa disconnected(), stateChanged().
 */

/*!
 *  @fn void Qmpi::responseReceived(QJsonValue value, std::any context)
 *
 *  This signal is emitted when the a successful (i.e. `return`) response is received from the server
 *  after executing a command.
 *
 *  @a value contains the return value of the command, which may be an empty QJsonObject if the command
 *  does not return data, while @a context contains the context object provided when the command was
 *  executed, if it was set.
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
 *  @fn void Qmpi::connectionErrorOccured(QAbstractSocket::SocketError error)
 *
 *  This signal is emitted when the underlying socket experiences a connection error, with @a error
 *  containing the type of error.
 *
 *  @note When such and error occurs the connection is immediately aborted, all pending reads/writes are discarded,
 *  and the interface enters the @ref Disconnected state.
 */

/*!
 *  @fn void Qmpi::communicationErrorOccured(Qmpi::CommunicationError error)
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
 *  command was executed, if it was set.
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
