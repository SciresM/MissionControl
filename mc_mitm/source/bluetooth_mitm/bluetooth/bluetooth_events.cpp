/*
 * Copyright (c) 2020-2021 ndeadly
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <switch.h>
#include "bluetooth_events.hpp"
#include "bluetooth_core.hpp"
#include "bluetooth_hid.hpp"
#include "bluetooth_ble.hpp"

namespace ams::bluetooth::events {

    namespace {

        os::ThreadType 						        g_event_handler_thread;
        alignas(os::ThreadStackAlignment) uint8_t   g_event_handler_thread_stack[0x2000];

        os::MultiWaitType g_manager;
        os::MultiWaitHolderType 	g_holder_bt_core;
        os::MultiWaitHolderType 	g_holder_bt_hid;
        os::MultiWaitHolderType 	g_holder_bt_ble;

        void EventHandlerThreadFunc(void *) {
            os::InitializeMultiWait(&g_manager);

            ams::bluetooth::core::WaitInitialized();
            os::InitializeMultiWaitHolder(&g_holder_bt_core, core::GetSystemEvent()->GetBase());
            os::SetMultiWaitHolderUserData(&g_holder_bt_core, BtdrvEventType_BluetoothCore);
            os::LinkMultiWaitHolder(&g_manager, &g_holder_bt_core);

            ams::bluetooth::hid::WaitInitialized();
            os::InitializeMultiWaitHolder(&g_holder_bt_hid, hid::GetSystemEvent()->GetBase());
            os::SetMultiWaitHolderUserData(&g_holder_bt_hid, BtdrvEventType_BluetoothHid);
            os::LinkMultiWaitHolder(&g_manager, &g_holder_bt_hid);

            if (hos::GetVersion() >= hos::Version_5_0_0) {
                ams::bluetooth::ble::WaitInitialized();
                os::InitializeMultiWaitHolder(&g_holder_bt_ble, ble::GetSystemEvent()->GetBase());
                os::SetMultiWaitHolderUserData(&g_holder_bt_ble, BtdrvEventType_BluetoothBle);
                os::LinkMultiWaitHolder(&g_manager, &g_holder_bt_ble);
            }

            while (true) {
                auto signalled_holder = os::WaitAny(&g_manager);
                switch (os::GetMultiWaitHolderUserData(signalled_holder)) {
                    case BtdrvEventType_BluetoothCore:
                        core::GetSystemEvent()->Clear();
                        core::HandleEvent();
                        break;
                    case BtdrvEventType_BluetoothHid:
                        hid::GetSystemEvent()->Clear();
                        hid::HandleEvent();
                        break;
                    case BtdrvEventType_BluetoothBle:
                        ble::GetSystemEvent()->Clear();
                        ble::HandleEvent();
                        break;
                    default:
                        break;
                }
            }
        }

    }

    Result Initialize(void) {
        R_TRY(os::CreateThread(&g_event_handler_thread,
            EventHandlerThreadFunc,
            nullptr,
            g_event_handler_thread_stack,
            sizeof(g_event_handler_thread_stack),
            9
        ));

        os::StartThread(&g_event_handler_thread);

        return ams::ResultSuccess();
    }

    void Finalize(void) {
        os::DestroyThread(&g_event_handler_thread);
    }

}
