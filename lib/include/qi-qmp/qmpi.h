#ifndef QMPI_H
#define QMPI_H

// Shared Lib Support
#include "qi-qmp/qmpi_export.h"

// Standard Library Includes
#include <any>
#include <queue>

// Qt Includes
#include <QObject>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

class QI_QMP_QMPI_EXPORT Qmpi : public QObject
{
    Q_OBJECT
//-Class Enums--------------------------------------------------------------------------------------------------------------
public:
    enum State { Disconnected, Connecting, AwaitingWelcome, Negotiating, Idle, Closing, SendingCommand, AwaitingMessage, ReadingMessage };
    enum CommunicationError { WriteFailed, ReadFailed, TransactionTimeout, UnexpectedReceive, UnexpectedResponse };

//-Class Structs------------------------------------------------------------------------------------------------------------
private:
    struct ExecutionTask
    {
        QString command;
        QJsonObject args;
        std::any context;
    };

//-Instance Variables--------------------------------------------------------------------------------------------------------
private:
    // Network
    std::variant<QHostAddress, QString> mHostId; static_assert(std::variant_size_v<decltype(mHostId)> == 2);
    quint16 mPort;
    QTcpSocket mSocket;

    // Workables
    State mState;
    std::queue<ExecutionTask> mExecutionQueue;

    // Timeout
    QTimer mTransactionTimer;

//-Constructor------------------------------------------------------------------------------------------------------------
public:
    explicit Qmpi(QObject* parent = nullptr);
    explicit Qmpi(const QHostAddress& address, quint16 port, QObject* parent = nullptr);
    explicit Qmpi(const QString& hostname, quint16 port, QObject* parent = nullptr);

//-Destructor-------------------------------------------------------------
public:
    ~Qmpi();

//-Instance Functions------------------------------------------------------------------------------------------------------
private:
    // Management
    void changeState(State newState);
    bool startTransactionTimer();
    bool stopTransactionTimer();
    void reset();
    void raiseCommunicationError(CommunicationError error);
    void finish();

    // Setup
    void negotiateCapabilities();

    // Commands
    bool sendCommand(QString command, QJsonObject args = QJsonObject());
    void propagate();

    // Message Processing
    void processServerMessage(const QJsonObject& jMsg);
    bool processGreetingMessage(const QJsonObject& jGreeting);
    bool processSuccessMessage(const QJsonObject& jSuccess);
    bool processErrorMessage(const QJsonObject& jError);
    bool processEventMessage(const QJsonObject& jEvent);

public:
    // Info
    State state() const;

    // Properties
    QHostAddress address() const;
    QString hostname() const;
    quint16 port() const;
    int transactionTimeout() const;

    void setAddress(const QHostAddress address);
    void setHostname(const QString hostname);
    void setPort(quint16 port);
    void setTransactionTimeout(int timeout = 30000);

    // Connection
    void connectToHost();
    void disconnectFromHost();
    void abort();
    bool isConnectionActive() const;
    bool isConnected() const;

    // Commands
    void execute(QString command, QJsonObject args = QJsonObject(), std::any context = std::any());

//-Signals & Slots------------------------------------------------------------------------------------------------------------
private slots:
    void handleSocketStateChange(QAbstractSocket::SocketState socketState);
    void handleReceivedData();
    void handleTransactionTimeout();

signals:
    void connected(QJsonObject version, QJsonArray capabilities);
    void readyForCommands();
    void disconnected(); // Only emitted if a connection to the server was made in the first place
    void finished(); // Emitted when the interface is fully closed, regardless of how it got there
    void responseReceived(QJsonValue value, std::any context);
    void eventReceived(QString name, QJsonObject data, QDateTime timestamp);
    void connectionErrorOccurred(QAbstractSocket::SocketError error); // Will be disconnected after
    void communicationErrorOccurred(Qmpi::CommunicationError error); // Will disconnect after
    void errorResponseReceived(QString errorClass, QString description, std::any context); // Will not disconnect after
    void stateChanged(Qmpi::State state);
};

#endif // QMPI_H
