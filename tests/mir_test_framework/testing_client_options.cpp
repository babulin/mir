/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir_test_framework/testing_client_configuration.h"
#include "mir/options/program_option.h"
#include "src/client/default_connection_configuration.h"
#include "src/client/client_platform_factory.h"
#include "src/client/client_buffer_factory.h"
#include "src/client/client_buffer.h"
#include "src/client/client_platform.h"
#include "src/client/mir_connection.h"

namespace mcl = mir::client;
namespace mtf=mir_test_framework;
namespace geom = mir::geometry;

/* if set before any calls to the api functions, assigning to this pointer will allow user to
 * override calls to mir_connect() and mir_connection_release(). This is mostly useful in test scenarios
 */
extern MirWaitHandle* (*mir_connect_impl)(
    char const *server,
    char const *app_name,
    mir_connected_callback callback,
    void *context);
extern void (*mir_connection_release_impl) (MirConnection *connection);

namespace
{
class StubClientBuffer : public mcl::ClientBuffer
{
    std::shared_ptr<mcl::MemoryRegion> secure_for_cpu_write()
    {
        return nullptr;
    }

    geom::Size size() const
    {
        return geom::Size{};
    }

    geom::Stride stride() const
    {
        return geom::Stride{};
    }

    geom::PixelFormat pixel_format() const
    {
        return geom::PixelFormat::abgr_8888;
    }
    
    uint32_t age() const
    {
        return 0;
    }
    void increment_age()
    {
    }
    void mark_as_submitted()
    {
    }
    std::shared_ptr<MirNativeBuffer> native_buffer_handle() const
    {
        return nullptr;
    }
};

struct StubClientBufferFactory : public mcl::ClientBufferFactory
{
    std::shared_ptr<mcl::ClientBuffer> create_buffer(std::shared_ptr<MirBufferPackage> const&,
                                                     geom::Size, geom::PixelFormat)
    {
        return std::make_shared<StubClientBuffer>();
    }
};

struct StubClientPlatform : public mcl::ClientPlatform
{
    MirPlatformType platform_type() const
    {
        return mir_platform_type_gbm;
    }

    std::shared_ptr<mcl::ClientBufferFactory> create_buffer_factory()
    {
        return std::make_shared<StubClientBufferFactory>();
    }

    std::shared_ptr<EGLNativeWindowType> create_egl_native_window(mcl::ClientSurface*)
    {
        auto fake_window = reinterpret_cast<EGLNativeWindowType>(0x12345678);
        return std::make_shared<EGLNativeWindowType>(fake_window);
    }

    std::shared_ptr<EGLNativeDisplayType> create_egl_native_display()
    {
        auto fake_display = reinterpret_cast<EGLNativeDisplayType>(0x12345678);
        return std::make_shared<EGLNativeDisplayType>(fake_display);
    }
};

struct StubClientPlatformFactory : public mcl::ClientPlatformFactory
{
    std::shared_ptr<mcl::ClientPlatform> create_client_platform(mcl::ClientContext*)
    {
        return std::make_shared<StubClientPlatform>();
    }
};

struct StubConnectionConfiguration : public mcl::DefaultConnectionConfiguration
{
    StubConnectionConfiguration(std::string const& socket_file)
        : DefaultConnectionConfiguration(socket_file)
    {
    }

    std::shared_ptr<mcl::ClientPlatformFactory> the_client_platform_factory() override
    {
        return std::make_shared<StubClientPlatformFactory>();
    }
};

MirWaitHandle* mir_connect_test_override(
    char const *socket_file,
    char const *app_name,
    mir_connected_callback callback,
    void *context)
{
    StubConnectionConfiguration conf(socket_file);
    auto connection = new MirConnection(conf);
    return connection->connect(app_name, callback, context);
}

void mir_connection_release_override(MirConnection *connection)
{
    auto wait_handle = connection->disconnect();
    wait_handle->wait_for_all();
    delete connection;
}

}

#if 0
mtf::TestingClientConfiguration::TestingClientConfiguration()
    : default_mir_connect_impl(mir_connect_impl),
      default_mir_connection_release_impl(mir_connection_release_impl)
{
}

void mtf::TestingClientConfiguration::use_default_connect_functions()
{
    mir_connect_impl = default_mir_connect_impl;
    mir_connection_release_impl = default_mir_connection_release_impl;
}

void mtf::TestingClientConfiguration::set_client_configuration(std::shared_ptr<mir::options::Option> const& options)
{
    if (!options->get("tests-use-real-graphics", false))
    {
        mir_connect_impl = mir_connect_test_override;
        mir_connection_release_impl = mir_connection_release_override;
    }
}

mtf::TestingClientConfiguration::~TestingClientConfiguration()
{
    use_default_connect_functions();
}
#endif
