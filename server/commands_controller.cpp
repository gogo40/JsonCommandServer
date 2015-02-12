/*
Json Command Server

COMMANDS CONTROLLER

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

#include "server/commands_controller.h"
#include "server/base_server.h"
#include "client/base_client.h"

static const JsonCommandServer::processCmd __g_default_server_commands[] = {
    JsonCommandServer::DefaultCommands::print_message,
    JsonCommandServer::DefaultCommands::print_message_status,
    JsonCommandServer::DefaultCommands::print_message_error,
    JsonCommandServer::DefaultCommands::process_identify,
    JsonCommandServer::DefaultCommands::process_peers_list,
    JsonCommandServer::DefaultCommands::send_message_to
};

void JsonCommandServer::execute_command(int cmd_type, JsonCommandServer::BaseController* w, const QJsonObject& cmd_args, const QJsonObject& full_command)
{
    if (cmd_type < N_CMDS) {
        __g_default_server_commands[cmd_type](w, cmd_args, full_command);
    }
}


void JsonCommandServer::DefaultCommands::print_message(JsonCommandServer::BaseController* w, const QJsonObject& cmd_args, const QJsonObject& full_command)
{
    QString msg;

    msg += QDate::currentDate().toString() + " " + QTime::currentTime().toString() + " ";

    if (full_command.contains("name_client")) {
        msg += full_command["name_client"].toString();
    }

    msg += "> ";

    if (cmd_args.contains("message")) {
        msg += cmd_args["message"].toString();
    }

    msg += "\n";

    w->addClientMessage(msg);
}

void JsonCommandServer::DefaultCommands::print_message_status(JsonCommandServer::BaseController* w, const QJsonObject& cmd_args, const QJsonObject& full_command)
{
    QString msg;

    msg += QDate::currentDate().toString() + " " + QTime::currentTime().toString() + " ";

    if (full_command.contains("name_client")) {
        msg += full_command["name_client"].toString() + "@";
    }

    if (full_command.contains("ip")) {
        msg += full_command["ip"].toString() + ":";
    }

    if (full_command.contains("port")) {
        msg += QString::number(full_command["port"].toInt()) + " ";
    }

    msg += "> ";

    if (cmd_args.contains("message")) {
        msg += cmd_args["message"].toString();
    }

    msg += "\n";

    w->addStatusMessage(msg);
}


void JsonCommandServer::DefaultCommands::print_message_error(JsonCommandServer::BaseController* w, const QJsonObject& cmd_args, const QJsonObject& full_command)
{
    QString msg;

    msg += QDate::currentDate().toString() + " " + QTime::currentTime().toString() + " ";

    if (full_command.contains("name_client")) {
        msg += full_command["name_client"].toString() + "@";
    }

    if (full_command.contains("ip")) {
        msg += full_command["ip"].toString() + ":";
    }

    if (full_command.contains("port")) {
        msg += QString::number(full_command["port"].toInt()) + " ";
    }

    msg += "> ";

    if (cmd_args.contains("message")) {
        msg += cmd_args["message"].toString();
    }

    msg += "\n";

    w->addErrorMessage(msg);
}

void JsonCommandServer::DefaultCommands::process_identify(JsonCommandServer::BaseController* w, const QJsonObject& cmd_args, const QJsonObject& full_command)
{
    w->addIdentify(full_command);
}

void JsonCommandServer::DefaultCommands::process_peers_list(JsonCommandServer::BaseController* w, const QJsonObject& cmd_args, const QJsonObject& full_command)
{
    if (full_command.contains("peers")) {
        QList<QString> peers;
        QJsonArray array = full_command["peers"].toArray();
        QString peers_str;

        for (int i = 0; i < array.size(); ++i) {
            peers.append(array[i].toString());
            peers_str += array[i].toString() + "\n";
        }

        w->addPeerList(peers);
    }
}

void JsonCommandServer::DefaultCommands::send_message_to(JsonCommandServer::BaseController* w, const QJsonObject& cmd_args, const QJsonObject& full_command)
{
    if (full_command.contains("from")) {
        QString from = full_command["from"].toString();
        if (full_command.contains("to")) {
            QString to = full_command["to"].toString();

            if (full_command.contains("message")) {
                w->sendMessageTo(from, to, full_command["message"].toString());
            }
        }
    }

}

JsonCommandServer::BaseController::BaseController()
{

}

JsonCommandServer::BaseController::~BaseController()
{

}
