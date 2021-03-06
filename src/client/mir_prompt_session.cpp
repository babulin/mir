/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2 or 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Nick Dedekind <nick.dedekind@canonical.com>
 */

#include "mir_prompt_session.h"
#include "event_handler_register.h"
#include "make_protobuf_object.h"
#include "rpc/mir_display_server.h"
#include "mir_protobuf.pb.h"

namespace mp = mir::protobuf;
namespace mcl = mir::client;
namespace mclr = mir::client::rpc;

MirPromptSession::MirPromptSession(
    mclr::DisplayServer& server,
    std::shared_ptr<mcl::EventHandlerRegister> const& event_handler_register) :
    server(server),
    parameters(mcl::make_protobuf_object<mir::protobuf::PromptSessionParameters>()),
    add_result(mcl::make_protobuf_object<mir::protobuf::Void>()),
    protobuf_void(mcl::make_protobuf_object<mir::protobuf::Void>()),
    socket_fd_response(mcl::make_protobuf_object<mir::protobuf::SocketFD>()),
    event_handler_register(event_handler_register),
    event_handler_register_id{event_handler_register->register_event_handler(
        [this](MirEvent const& event)
        {
            if (mir_event_get_type(&event) == mir_event_type_prompt_session_state_change)
                set_state(mir_prompt_session_event_get_state(mir_event_get_prompt_session_event(&event)));
        })},
    state(mir_prompt_session_state_stopped),
    session(mcl::make_protobuf_object<mir::protobuf::PromptSession>()),
    handle_prompt_session_state_change{[](MirPromptSessionState){}}
{
}

MirPromptSession::~MirPromptSession()
{
    set_state(mir_prompt_session_state_stopped);
}

void MirPromptSession::set_state(MirPromptSessionState new_state)
{
    std::lock_guard<decltype(event_handler_mutex)> lock(event_handler_mutex);

    if (new_state != state)
    {
        handle_prompt_session_state_change(new_state);

        if (new_state == mir_prompt_session_state_stopped)
        {
            event_handler_register->unregister_event_handler(event_handler_register_id);
        }

        state = new_state;
    }
}

MirWaitHandle* MirPromptSession::start(pid_t application_pid, MirPromptSessionCallback callback, void* context)
{
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        parameters->set_application_pid(application_pid);
    }

    start_wait_handle.expect_result();
    server.start_prompt_session(
        parameters.get(),
        session.get(),
        google::protobuf::NewCallback(this, &MirPromptSession::done_start,
                                      callback, context));

    return &start_wait_handle;
}

MirWaitHandle* MirPromptSession::stop(MirPromptSessionCallback callback, void* context)
{
    stop_wait_handle.expect_result();

    server.stop_prompt_session(
        session.get(),
        protobuf_void.get(),
        google::protobuf::NewCallback(this, &MirPromptSession::done_stop,
                                      callback, context));

    return &stop_wait_handle;
}

void MirPromptSession::register_prompt_session_state_change_callback(
    MirPromptSessionStateChangeCallback callback,
    void* context)
{
    std::lock_guard<decltype(event_handler_mutex)> lock(event_handler_mutex);

    handle_prompt_session_state_change =
        [this, callback, context](MirPromptSessionState new_state)
        {
            callback(this, new_state, context);
        };
}

void MirPromptSession::done_start(MirPromptSessionCallback callback, void* context)
{
    {
        std::lock_guard<decltype(session_mutex)> lock(session_mutex);

        state = session->has_error() ? mir_prompt_session_state_stopped : mir_prompt_session_state_started;
    }

    callback(this, context);
    start_wait_handle.result_received();
}

void MirPromptSession::done_stop(MirPromptSessionCallback callback, void* context)
{
    set_state(mir_prompt_session_state_stopped);

    callback(this, context);
    stop_wait_handle.result_received();
}

char const* MirPromptSession::get_error_message()
{
    std::lock_guard<decltype(session_mutex)> lock(session_mutex);

    if (!session->has_error())
        session->set_error(std::string{});

    return session->error().c_str();
}

MirWaitHandle* MirPromptSession::new_fds_for_prompt_providers(
    unsigned int no_of_fds,
    MirClientFdCallback callback,
    void * context)
{
    mp::SocketFDRequest request;
    request.set_number(no_of_fds);
    request.set_prompt_session_id(session->id());

    fds_for_prompt_providers_wait_handle.expect_result();

    server.new_fds_for_prompt_providers(
        &request,
        socket_fd_response.get(),
        google::protobuf::NewCallback(this, &MirPromptSession::done_fds_for_prompt_providers,
                                              callback, context));

    return &fds_for_prompt_providers_wait_handle;
}

void MirPromptSession::done_fds_for_prompt_providers(
    MirClientFdCallback callback,
    void* context)
{
    auto const size = socket_fd_response->fd_size();

    std::vector<int> fds;
    fds.reserve(size);

    for (auto i = 0; i != size; ++i)
        fds.push_back(socket_fd_response->fd(i));

    callback(this, size, fds.data(), context);
    fds_for_prompt_providers_wait_handle.result_received();
}
