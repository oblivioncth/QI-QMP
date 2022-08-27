#ifndef QMPI_H
#define QMPI_H

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

class Qmpi : public QObject
{
    Q_OBJECT
//-Class Enums--------------------------------------------------------------------------------------------------------------
public:
    enum State { Disconnected, Connecting, AwaitingWelcome, Negotiating, Idle, Closing, ExecutingCommand, AwaitingMessage, ReadingMessage };
    enum CommunicationError { WriteFailed, ReadFailed, TransactionTimeout, UnexpectedReceive, UnexpectedResponse };

//-Class Variables-----------------------------------------------------------------------------------------------------------
private:
    /*! @cond */
    class JsonKeys
    {
    public:
        static inline const QString GREETING = "QMP";
        class Greeting
        {
        public:
            static inline const QString VERSION = "version";
            static inline const QString CAPABILITIES = "capabilities";
        };

        static inline const QString RETURN = "return";
        static inline const QString ERROR = "error";
        class Error
        {
        public:
            static inline const QString CLASS = "class";
            static inline const QString DESCRIPTION = "desc";
        };

        static inline const QString EVENT = "event";
        class Event
        {
        public:
            static inline const QString DATA = "data";
            static inline const QString TIMESTAMP = "timestamp";
            class Timestamp
            {
            public:
                static inline const QString SECONDS = "seconds";
                static inline const QString MICROSECONDS = "microseconds";
            };
        };

        static inline const QString EXECUTE = "execute";
        class Execute
        {
        public:
            static inline const QString ARGUMENTS = "arguments";
        };
    };
    /*! @endcond */
    static inline const QString NEGOTIATION_COMMAND = "qmp_capabilities";

//-Instance Variables--------------------------------------------------------------------------------------------------------
private:
    // Network
    std::variant<QHostAddress, QString> mHostId; static_assert(std::variant_size_v<decltype(mHostId)> == 2);
    quint16 mPort;
    QTcpSocket mSocket;

    // Workables
    State mState;
    std::queue<std::any> mResponseAwaitQueue;

    // Timeout
    QTimer mTransactionTimer;

//-Constructor------------------------------------------------------------------------------------------------------------
private:
    explicit Qmpi(quint16 port, QObject* parent);

public:
    explicit Qmpi(const QHostAddress& address, quint16 port, QObject* parent = nullptr);
    explicit Qmpi(const QString& hostname, quint16 port, QObject* parent = nullptr);

//-Destructor-------------------------------------------------------------
public:
    ~Qmpi();

//-Instance Functions------------------------------------------------------------------------------------------------------
private:
    // Management
    void changeState(State newState);
    void startTransactionTimer();
    void stopTransactionTimer();
    void reset();
    void raiseCommunicationError(CommunicationError error);

    // Setup
    void negotiateCapabilities();

    // Commands
    bool sendCommand(QString command, QJsonObject args = QJsonObject());

    // Message Processing
    void processServerMessage(const QJsonObject& msg);
    bool processGreetingMessage(const QJsonObject& greeting);
    bool processReturnMessage(const QJsonObject& ret);
    bool processErrorMessage(const QJsonObject& error);
    bool processEventMessage(const QJsonObject& event);

public:
    // Info
    QHostAddress address() const;
    QString hostname() const;
    quint16 port() const;
    State state() const;

    // Properties
    void setTransactionTimeout(int timeout = 30000);
    int transactionTimeout() const;

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
    void connectionErrorOccured(QAbstractSocket::SocketError error); // Will be disconnected after
    void communicationErrorOccured(Qmpi::CommunicationError error); // Will disconnect after
    void errorResponseReceived(QString errorClass, QString description, std::any context); // Will not disconnect after
    void stateChanged(Qmpi::State state);
};

#endif // QMPI_H
