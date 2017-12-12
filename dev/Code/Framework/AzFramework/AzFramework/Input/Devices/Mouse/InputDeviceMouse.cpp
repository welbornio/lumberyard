/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Input/Utils/ProcessRawInputEventQueues.h>

#include <AzCore/std/smart_ptr/make_shared.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputDeviceId InputDeviceMouse::Id("mouse");

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputChannelId InputDeviceMouse::Button::Left("mouse_button_left");
    const InputChannelId InputDeviceMouse::Button::Right("mouse_button_right");
    const InputChannelId InputDeviceMouse::Button::Middle("mouse_button_middle");
    const InputChannelId InputDeviceMouse::Button::Other1("mouse_button_other1");
    const InputChannelId InputDeviceMouse::Button::Other2("mouse_button_other2");
    const AZStd::array<InputChannelId, 5> InputDeviceMouse::Button::All =
    {{
        Left,
        Right,
        Middle,
        Other1,
        Other2
    }};

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputChannelId InputDeviceMouse::Movement::X("mouse_delta_x");
    const InputChannelId InputDeviceMouse::Movement::Y("mouse_delta_y");
    const InputChannelId InputDeviceMouse::Movement::Z("mouse_delta_z");
    const AZStd::array<InputChannelId, 3> InputDeviceMouse::Movement::All =
    {{
        X,
        Y,
        Z
    }};

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputChannelId InputDeviceMouse::SystemCursorPosition("mouse_system_cursor_position");

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceMouse::Implementation::CustomCreateFunctionType InputDeviceMouse::Implementation::CustomCreateFunctionPointer = nullptr;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceMouse::InputDeviceMouse()
        : InputDevice(Id)
        , m_allChannelsById()
        , m_buttonChannelsById()
        , m_movementChannelsById()
        , m_cursorPositionChannel()
        , m_cursorPositionData2D(AZStd::make_shared<InputChannel::PositionData2D>())
        , m_pimpl(nullptr)
    {
        // Create all button input channels
        for (const InputChannelId& channelId : Button::All)
        {
            InputChannelDigitalWithSharedPosition2D* channel = aznew InputChannelDigitalWithSharedPosition2D(channelId, *this, m_cursorPositionData2D);
            m_allChannelsById[channelId] = channel;
            m_buttonChannelsById[channelId] = channel;
        }

        // Create all movement input channels
        for (const InputChannelId& channelId : Movement::All)
        {
            InputChannelDeltaWithSharedPosition2D* channel = aznew InputChannelDeltaWithSharedPosition2D(channelId, *this, m_cursorPositionData2D);
            m_allChannelsById[channelId] = channel;
            m_movementChannelsById[channelId] = channel;
        }

        // Create the cursor position input channel
        m_cursorPositionChannel = aznew InputChannelDeltaWithSharedPosition2D(SystemCursorPosition, *this, m_cursorPositionData2D);
        m_allChannelsById[SystemCursorPosition] = m_cursorPositionChannel;

        // Create the platform specific implementation
        m_pimpl = Implementation::CustomCreateFunctionPointer ?
                  Implementation::CustomCreateFunctionPointer(*this) :
                  Implementation::Create(*this);

        // Connect to the system cursor request bus
        InputSystemCursorRequestBus::Handler::BusConnect(GetInputDeviceId());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceMouse::~InputDeviceMouse()
    {
        // Disconnect from the system cursor request bus
        InputSystemCursorRequestBus::Handler::BusDisconnect(GetInputDeviceId());

        // Destroy the platform specific implementation
        delete m_pimpl;

        // Destroy the cursor position input channel
        delete m_cursorPositionChannel;

        // Destroy all movement input channels
        for (const auto& channelById : m_movementChannelsById)
        {
            delete channelById.second;
        }

        // Destroy all button input channels
        for (const auto& channelById : m_buttonChannelsById)
        {
            delete channelById.second;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const InputDevice::InputChannelByIdMap& InputDeviceMouse::GetInputChannelsById() const
    {
        return m_allChannelsById;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    bool InputDeviceMouse::IsSupported() const
    {
        return m_pimpl != nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    bool InputDeviceMouse::IsConnected() const
    {
        return m_pimpl ? m_pimpl->IsConnected() : false;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceMouse::TickInputDevice()
    {
        if (m_pimpl)
        {
            m_pimpl->TickInputDevice();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceMouse::SetSystemCursorState(SystemCursorState systemCursorState)
    {
        if (m_pimpl)
        {
            m_pimpl->SetSystemCursorState(systemCursorState);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    SystemCursorState InputDeviceMouse::GetSystemCursorState() const
    {
        return m_pimpl ? m_pimpl->GetSystemCursorState() : SystemCursorState::Unknown;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceMouse::SetSystemCursorPositionNormalized(AZ::Vector2 positionNormalized)
    {
        if (m_pimpl)
        {
            m_pimpl->SetSystemCursorPositionNormalized(positionNormalized);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    AZ::Vector2 InputDeviceMouse::GetSystemCursorPositionNormalized() const
    {
        return m_pimpl ? m_pimpl->GetSystemCursorPositionNormalized() : AZ::Vector2::CreateZero();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceMouse::Implementation::Implementation(InputDeviceMouse& inputDevice)
        : m_inputDevice(inputDevice)
        , m_rawButtonEventQueuesById()
        , m_rawMovementEventQueuesById()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    InputDeviceMouse::Implementation::~Implementation()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceMouse::Implementation::QueueRawButtonEvent(const InputChannelId& inputChannelId,
                                                               bool rawButtonState)
    {
        // It should not (in theory) be possible to receive multiple button events with the same id
        // and state in succession; if it happens in practice for whatever reason this is still safe.
        m_rawButtonEventQueuesById[inputChannelId].push_back(rawButtonState);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceMouse::Implementation::QueueRawMovementEvent(const InputChannelId& inputChannelId,
                                                                 float rawMovementDelta)
    {
        // Raw mouse movement is coalesced rather than queued to avoid flooding the event queue
        auto& rawEventQueue = m_rawMovementEventQueuesById[inputChannelId];
        if (rawEventQueue.empty())
        {
            rawEventQueue.push_back(rawMovementDelta);
        }
        else
        {
            rawEventQueue.back() += rawMovementDelta;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceMouse::Implementation::ProcessRawEventQueues()
    {
        // Update the shared cursor position data
        const AZ::Vector2 oldNormalizedPosition = m_inputDevice.m_cursorPositionData2D->m_normalizedPosition;
        const AZ::Vector2 newNormalizedPosition = GetSystemCursorPositionNormalized();
        m_inputDevice.m_cursorPositionData2D->m_normalizedPosition = newNormalizedPosition;
        m_inputDevice.m_cursorPositionData2D->m_normalizedPositionDelta = newNormalizedPosition - oldNormalizedPosition;

        // Process all raw input events that were queued since the last call to this function
        ProcessRawInputEventQueues(m_rawButtonEventQueuesById, m_inputDevice.m_buttonChannelsById);
        ProcessRawInputEventQueues(m_rawMovementEventQueuesById, m_inputDevice.m_movementChannelsById);

        // Mouse movement events are distinct in that we may not receive an 'ended' event with delta
        // value of zero when the mouse stops moving, so queueing one here ensures the channels will
        // always correctly transition into the 'ended' state the next time this function is called,
        // unless another movement delta is queued above in which case it will simply be added to 0.
        for (const InputChannelId& movementChannelId : Movement::All)
        {
            QueueRawMovementEvent(movementChannelId, 0.0f);
        }

        // Finally, update the cursor position input channel, treating it as active if it has moved
        const float distanceMoved = newNormalizedPosition.GetDistance(oldNormalizedPosition);
        m_inputDevice.m_cursorPositionChannel->ProcessRawInputEvent(distanceMoved);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void InputDeviceMouse::Implementation::ResetInputChannelStates()
    {
        m_inputDevice.ResetInputChannelStates();
    }
} // namespace AzFramework