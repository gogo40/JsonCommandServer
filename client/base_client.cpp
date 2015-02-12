/*
Json Command Server

BASE CLIENT

Copyright (c) 2015, gogo40, Péricles Lopes Machado <pericles.raskolnikoff@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or other
materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may
be used to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "client/base_client.h"

#include <QJsonDocument>


static const int N_MAX_SERVER_MESSAGES = 50;

JsonCommandServer::BaseClient::BaseClient(QObject *_parent)
    : QObject(_parent), BaseController(),
      client_tcp_socket_(0),
      client_current_message_(""),
      client_block_size_(0),
      client_network_session_(0),
      next_id_(0),
      n_messages_(0)
{
}

JsonCommandServer::BaseClient::~BaseClient()
{

}

void JsonCommandServer::BaseClient::setServerHost(const QString &_host)
{
    server_ip_ = _host;
}

void JsonCommandServer::BaseClient::setServerPort(int _port)
{
    server_port_ = _port;
    client_port_ = _port + 1;
}

int JsonCommandServer::BaseClient::myServerPort()
{
    return server_port_;
}

QString JsonCommandServer::BaseClient::myServerIP()
{
    return server_ip_;
}

/*Client slots*/

void JsonCommandServer::BaseClient::clientConnect()
{
    clientClose();
    clientInit();
    clientRequestNewInitialMessage();
}

void JsonCommandServer::BaseClient::clientClose()
{
    if (client_tcp_socket_) {
        writeMessage(client_tcp_socket_, processMessage("close"));
        client_tcp_socket_->close();
        delete client_tcp_socket_;
    }
    if (client_network_session_) delete client_network_session_;

    client_tcp_socket_ = 0;
    client_network_session_ = 0;
    client_block_size_ = 0;
    next_id_ = 0;

    ips_info_.clear();

    this->updateInfos();
    clearMessages();
    enableClient();
}

void JsonCommandServer::BaseClient::clientInit()
{
    clearMessages();
    enableClient();

    // find out name of this machine
    QString name = QHostInfo::localHostName();

    addStatusMessage("Host IP: " + myServerIP() + ": " + QString::number(myServerPort()));
    addStatusMessage("IPs disponiveis:");

    if (!name.isEmpty()) {
        addStatusMessage(name);

        QString domain = QHostInfo::localDomainName();
        if (!domain.isEmpty()) {
            addStatusMessage(name + QChar('.') + domain);
        }
    }

    if (name != QString("localhost")) {
         addStatusMessage(QString("localhost"));
    }

    // find out IP addresses of this machine
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    // add non-localhost addresses
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (!ipAddressesList.at(i).isLoopback()) {
            addStatusMessage(ipAddressesList.at(i).toString());
        }
    }

    // add localhost addresses
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (ipAddressesList.at(i).isLoopback()) {
            addStatusMessage(ipAddressesList.at(i).toString());
        }
    }

    disableClient();

    client_tcp_socket_ = new QTcpSocket(this);

    connect(client_tcp_socket_, SIGNAL(readyRead()), this, SLOT(clientReadMessage()));
    connect(client_tcp_socket_, SIGNAL(error(QAbstractSocket::SocketError)),
    this, SLOT(clientDisplayError(QAbstractSocket::SocketError)));


    QNetworkConfigurationManager manager;
    if (manager.capabilities() & QNetworkConfigurationManager::NetworkSessionRequired) {
        // Get saved network configuration
        QSettings settings(QSettings::UserScope, QLatin1String("MiningControlServerTest"));
        settings.beginGroup(QLatin1String("MiningControlServerTestNetwork"));
        const QString id = settings.value(QLatin1String("DefaultNetworkConfiguration")).toString();
        settings.endGroup();

        // If the saved network configuration is not currently discovered use the system default
        QNetworkConfiguration config = manager.configurationFromIdentifier(id);
        if ((config.state() & QNetworkConfiguration::Discovered) !=
        QNetworkConfiguration::Discovered) {
            config = manager.defaultConfiguration();
        }

        client_network_session_ = new QNetworkSession(config, this);
        connect(client_network_session_, SIGNAL(opened()), this, SLOT(clientSessionOpened()));

        disableClient();

        addStatusMessage(tr("Abrindo sessão de rede."));
        client_network_session_->open();
    }
}

void JsonCommandServer::BaseClient::clientRequestNewInitialMessage()
{
    disableClient();

    client_block_size_ = 0;
    client_tcp_socket_->abort();
    client_tcp_socket_->connectToHost(myServerIP(),
                                 myServerPort());
}

void JsonCommandServer::BaseClient::clientReadMessage()
{
    QDataStream in(client_tcp_socket_);
    in.setVersion(QDataStream::Qt_4_0);

    if (client_tcp_socket_->bytesAvailable() < (int)sizeof(quint16)) {
        client_tcp_socket_->readAll();
        return;
    }

    in >> client_block_size_;

    if (client_tcp_socket_->bytesAvailable() < client_block_size_) {
        client_tcp_socket_->readAll();
        return;
    }

    QString message;
    in >> message;

    if (message == client_current_message_) {
        client_tcp_socket_->readAll();
        return;
    }

    client_current_message_ = message;

    processMessage(client_tcp_socket_, message);

    enableClient();

    newMessage();
    client_tcp_socket_->readAll();
}

void JsonCommandServer::BaseClient::clientDisplayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        addErrorMessage(tr("Host fechou a conexão."));
        break;
    case QAbstractSocket::HostNotFoundError:
        addErrorMessage(tr("Host não encontrado. Por favor, verifique as configurações "
                                    "de host e porta."));
        break;
    case QAbstractSocket::ConnectionRefusedError:
        addErrorMessage(tr("A conexão foi recusado pelo servidor. "
                                    "Verifique se o servidor está rodando, "
                                    "e confirme as configurações de host e porta. "));
        break;
    default:
        addErrorMessage(tr("Um erro ocorreu: %1.")
                                 .arg(client_tcp_socket_->errorString()));
    }

    enableClient();
}


void JsonCommandServer::BaseClient::clientSessionOpened()
{
    // Save the used configuration
    QNetworkConfiguration config = client_network_session_->configuration();
    QString id;
    if (config.type() == QNetworkConfiguration::UserChoice)
        id = client_network_session_->sessionProperty(QLatin1String("UserChoiceConfiguration")).toString();
    else
        id = config.identifier();

    QSettings settings(QSettings::UserScope, QLatin1String("MiningControlServerTest"));
    settings.beginGroup(QLatin1String("MiningControlServerTestNetwork"));
    settings.setValue(QLatin1String("DefaultNetworkConfiguration"), id);
    settings.endGroup();

    addStatusMessage(tr("O servidor precisa está rodando."));

    enableClient();
}

void JsonCommandServer::BaseClient::clientSendMessage(const QString& _message)
{
    if (client_tcp_socket_) {
        writeMessage(client_tcp_socket_, processMessage(_message));
    }
}

void JsonCommandServer::BaseClient::clientIdentify()
{
    if (client_tcp_socket_) {
        writeMessage(client_tcp_socket_, createIdentify());
    }
}

QJsonArray JsonCommandServer::BaseClient::convertMessage(const QString &message, bool &ok)
{
    ok = false;
    QJsonArray out;
    std::string str = message.toStdString();
    QByteArray json_str(str.c_str(), str.size());
    QJsonDocument doc = QJsonDocument::fromJson(json_str);
    if (!doc.isNull()) {
        ok = true;
        return doc.array();
    }
    return out;
}

QJsonArray JsonCommandServer::BaseClient::createMessage(const QString &message, bool &ok, int type_message)
{
    ok = true;
    QJsonArray out;
    QJsonObject cmd;
    QJsonObject args;

    if (message == "close") {
        cmd.insert("type", CLOSE);
    } else {
        args["message"] = message;


        cmd.insert("id", newKey());
        cmd.insert("type", type_message);
        cmd.insert("time", QTime::currentTime().toString());
        cmd.insert("date", QDate::currentDate().toString());
        cmd.insert("id_client", this->id());
        cmd.insert("group_client", this->group());
        cmd.insert("name_client", this->name());
        cmd.insert("type_client", this->type());
        cmd.insert("args", args);
    }

    out.append(cmd);

    return out;
}

QJsonArray JsonCommandServer::BaseClient::createStatus(const QString &message, bool &ok)
{
    return createMessage(message, ok, MESSAGE_STATUS);
}

QJsonArray JsonCommandServer::BaseClient::createError(const QString &message, bool &ok)
{
    return createMessage(message, ok, MESSAGE_ERROR);
}

QJsonArray JsonCommandServer::BaseClient::createIdentify()
{
    QJsonArray out;
    QJsonObject cmd;

    cmd.insert("id", newKey());
    cmd.insert("type", MESSAGE_IDENTIFY);
    cmd.insert("time", QTime::currentTime().toString());
    cmd.insert("date", QDate::currentDate().toString());
    cmd.insert("id_client", this->id());
    cmd.insert("group_client", this->group());
    cmd.insert("name_client", this->name());
    cmd.insert("type_client", this->type());
    cmd.insert("description_client", this->description());

    out.append(cmd);

    return out;
}

QJsonArray JsonCommandServer::BaseClient::createPeerList()
{
    QJsonArray out;
    return out;
}

QJsonArray JsonCommandServer::BaseClient::createMessageTo(const QString &from, const QString &to, const QString &message)
{
    QJsonArray out;
    QJsonObject cmd;

    cmd.insert("id", newKey());
    cmd.insert("type", MESSAGE_TO);
    cmd.insert("time", QTime::currentTime().toString());
    cmd.insert("date", QDate::currentDate().toString());
    cmd.insert("id_client", this->id());
    cmd.insert("group_client", this->group());
    cmd.insert("name_client", this->name());
    cmd.insert("type_client", this->type());
    cmd.insert("description_client", this->description());
    cmd.insert("from", from);
    cmd.insert("to", to);
    cmd.insert("message", message);

    out.append(cmd);
    return out;

}

void JsonCommandServer::BaseClient::writeMessage(QTcpSocket *_socket, const QJsonArray &cmd)
{
    writeMessage(_socket, QJsonDocument(cmd).toJson());
}

QString JsonCommandServer::BaseClient::processMessage(const QString &_message)
{
    bool ok = false;
    QJsonArray cmd = createMessage(_message, ok, MESSAGE_NORMAL);

    if (!ok) {
        return "";
    }

    return QJsonDocument(cmd).toJson();
}

void JsonCommandServer::BaseClient::processMessage(QTcpSocket *_socket, const QString &message)
{
    bool ok;
    QJsonArray cmds = convertMessage(message, ok);

    if (ok) {
        for (int i  = 0; i < cmds.size(); ++i) {
            QJsonObject cmd = cmds[i].toObject();
            int type = -1;
            if (cmd.contains("type")) {
                type = cmd["type"].toInt();
            }

            cmd.insert("IP", _socket->peerAddress().toString());
            cmd.insert("port", QString::number(_socket->peerPort()));

            if (type == -1) continue;

            if (type == CLOSE) {
                clientClose();
            } else {
                if (cmd.contains("args")) {
                    execute_command(type, this, cmd["args"].toObject(), cmd);
                } else {
                    execute_command(type, this, cmd, cmd);
                }
            }
        }
    } else {
        addErrorMessage("Falha ao processar comando no client: " + message);
    }
}

void JsonCommandServer::BaseClient::writeMessage(QTcpSocket *_socket, const QString & _message)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    out.setVersion(QDataStream::Qt_4_0);

    out << (quint16)0;

    out << _message;

    out.device()->seek(0);
    out << (quint16)(block.size() - sizeof(quint16));

    _socket->write(block);
}

void JsonCommandServer::BaseClient::sendMessageTo(const QString& from, const QString &to, const QString &message)
{
    if (client_tcp_socket_) {
        if (message == "close") {
            clientClose();
        } else {
            writeMessage(client_tcp_socket_, createMessageTo(from, to, message));
        }
    }
}

void JsonCommandServer::BaseClient::addNewInfo(const RemoteNodeInfo &new_info)
{
    RemoteNodeInfo& info = ips_info_[new_info.IP][new_info.port];

    info.date = new_info.date;
    info.description = new_info.description;
    info.group = new_info.group;
    info.id = new_info.id;
    info.IP = new_info.IP;
    info.name = new_info.name;
    info.port = new_info.port;
    info.time = new_info.time;
    info.type = new_info.type;

    this->updateInfos();
}

int JsonCommandServer::BaseClient::newKey()
{
    ++next_id_;
    return next_id_;
}

void JsonCommandServer::BaseClient::newMessage()
{
    ++n_messages_;

    if (n_messages_ > N_MAX_SERVER_MESSAGES) {
        n_messages_ = 0;
        clearMessages();
    }
}

