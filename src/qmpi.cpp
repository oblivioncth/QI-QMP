// Unit Include
#include "qmpi.h"

// Qx Includes
#include <qx/core/qx-json.h>
#include <qx/core/qx-setonce.h>

//===============================================================================================================
// Qmpi
//===============================================================================================================

//-Constructor-------------------------------------------------------------
Qmpi::Qmpi(QHostAddress address, quint16 port, QObject* parent) :
      QObject(parent),
      mAddress(address),
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

//-Instance Functions------------------------------------------------------------------------------------------------------
//Private:
void Qmpi::changeState(State newState)
{
    mState = newState;
    emit stateChanged(mState);
}

void Qmpi::startTransactionTimer()
{
    /* Treat negative values as disabling the timeout. This must be done explicitly as starting
     * a QTimer with a negative interval causes a warning to be logged stating the interval
     * cannot be negative, even though it seems to work fine
     *
     * Also don't restart the timer if it's already running
     */
    if(mTransactionTimer.interval() >= 0 && !mTransactionTimer.isActive())
        mTransactionTimer.start();

}
void Qmpi::stopTransactionTimer()
{
    // Used to enforce symmetry for both starting and stopping the timer (i.e. not interacting with it directly)
    if(mTransactionTimer.isActive())
        mTransactionTimer.stop();
}

void Qmpi::reset()
{
    mResponseAwaitQueue = {};
    stopTransactionTimer();
}

void Qmpi::raiseCommunicationError(CommunicationError error)
{
    emit communicationErrorOccured(error);
    abort();
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

void Qmpi::processServerMessage(const QJsonObject& msg)
{
    // Stop transaction timer since message has arrived
    stopTransactionTimer();

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
        }
        else
        {
            // Error if not expecting a response
            if(mResponseAwaitQueue.empty())
            {
                raiseCommunicationError(CommunicationError::UnexpectedReceive);
                return;
            }

            // Check for each response type
            if(msg.contains(JsonKeys::RETURN))
            {
                if(!processReturnMessage(msg))
                    return;
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
            }
            else
            {
                raiseCommunicationError(CommunicationError::UnexpectedResponse);
                return;
            }
        }

        // Handle state/timer based on queue
        bool willIdle = mResponseAwaitQueue.empty();
        changeState(willIdle ? State::Idle : State::AwaitingMessage);
        if(!willIdle)
            startTransactionTimer();
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
    Q_ASSERT(!mResponseAwaitQueue.empty());

    QJsonValue value = ret[JsonKeys::RETURN];
    std::any& context = mResponseAwaitQueue.front();
    emit responseReceived(value, context);
    mResponseAwaitQueue.pop();
    return true; // Can't fail as of yet
}

bool Qmpi::processErrorMessage(const QJsonObject& error)
{
    Q_ASSERT(!mResponseAwaitQueue.empty());

    std::any& context = mResponseAwaitQueue.front();

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
    mResponseAwaitQueue.pop();
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
QHostAddress Qmpi::address() const { return mAddress; }
quint16 Qmpi::port() const { return mPort; }
Qmpi::State Qmpi::state() const { return mState; }

void Qmpi::setTimeout(int timeout) { mTransactionTimer.setInterval(timeout); }
int Qmpi::timeout() const { return mTransactionTimer.interval(); }

void Qmpi::connectToHost()
{
    // Bail if already working
    if(isConnectionActive())
        return;

    // Connect
    changeState(State::Connecting);
    mSocket.connectToHost(mAddress, mPort);
}

void Qmpi::disconnectFromHost()
{
    // Bail if already disconnected
    if(!isConnectionActive())
        return;

    // Disconnect
    mSocket.disconnectFromHost();
}

bool Qmpi::isConnectionActive() const { return mState != State::Disconnected; }

void Qmpi::execute(QString command, QJsonObject args, std::any context)
{
    changeState(State::ExecutingCommand);

    // Add context to queue
    mResponseAwaitQueue.push(context);

    // Send command
    sendCommand(command, args);

    // Start timeout timer
    startTransactionTimer();

    // Set new state
    changeState(State::AwaitingMessage);
}

//-Signals & Slots------------------------------------------------------------------------------------------------------------
//Private Slots:
void Qmpi::handleSocketStateChange(QAbstractSocket::SocketState socketState)
{
    /* Provides finer awareness of socket state
     * i.e. disconnected isn't emitted if connection fails so if only using that signal cleanup may also
     * need to be prompted from the slot connected to errorOccured by explicitly checking for a failed connection
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
