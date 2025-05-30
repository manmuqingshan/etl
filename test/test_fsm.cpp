/******************************************************************************
The MIT License(MIT)

Embedded Template Library.
https://github.com/ETLCPP/etl
https://www.etlcpp.com

Copyright(c) 2017 John Wellbelove

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/

#include "unit_test_framework.h"

#include "etl/fsm.h"
#include "etl/enum_type.h"
#include "etl/container.h"
#include "etl/packet.h"
#include "etl/queue.h"

#include <iostream>
#include <limits>

namespace
{
  const etl::message_router_id_t Motor_Control = 0;


  //***************************************************************************
  // Events
  struct EventId
  {
    enum enum_type
    {
      Start,
      Stop,
      Stopped,
      Set_Speed,
      Recursive,
      Self_Transition,
      Unsupported
    };

    ETL_DECLARE_ENUM_TYPE(EventId, etl::message_id_t)
    ETL_ENUM_TYPE(Start,           "Start")
    ETL_ENUM_TYPE(Stop,            "Stop")
    ETL_ENUM_TYPE(Stopped,         "Stopped")
    ETL_ENUM_TYPE(Set_Speed,       "Set Speed")
    ETL_ENUM_TYPE(Recursive,       "Recursive")
    ETL_ENUM_TYPE(Self_Transition, "Self Transition")
    ETL_ENUM_TYPE(Unsupported,     "Unsupported")
    ETL_END_ENUM_TYPE
  };

  //***********************************
  class Start : public etl::message<EventId::Start>
  {
  };

  //***********************************
  class Stop : public etl::message<EventId::Stop>
  {
  public:

    Stop() : isEmergencyStop(false) {}
    Stop(bool emergency) : isEmergencyStop(emergency) {}

    const bool isEmergencyStop;
  };

  //***********************************
  class SetSpeed : public etl::message<EventId::Set_Speed>
  {
  public:

    SetSpeed(int speed_) : speed(speed_) {}

    const int speed;
  };

  //***********************************
  class Stopped : public etl::message<EventId::Stopped>
  {
  };

  //***********************************
  class Recursive : public etl::message<EventId::Recursive>
  {
  };

  //***********************************
  class SelfTransition : public etl::message<EventId::Self_Transition>
  {
  };

  //***********************************
  class Unsupported : public etl::message<EventId::Unsupported>
  {
  };

  //***************************************************************************
  // States
  struct StateId
  {
    enum enum_type
    {
      Idle,
      Running,
      Winding_Down,
      Locked,
      Number_Of_States
    };

    ETL_DECLARE_ENUM_TYPE(StateId, etl::fsm_state_id_t)
    ETL_ENUM_TYPE(Idle,         "Idle")
    ETL_ENUM_TYPE(Running,      "Running")
    ETL_ENUM_TYPE(Winding_Down, "Winding Down")
    ETL_ENUM_TYPE(Locked,       "Locked")
    ETL_END_ENUM_TYPE
  };

  //***********************************
  // The motor control FSM.
  //***********************************
  class MotorControl : public etl::fsm
  {
  public:

    MotorControl()
      : fsm(Motor_Control)
    {
    }

    //***********************************
    void Initialise(etl::ifsm_state** p_states, size_t size)
    {
      set_states(p_states, size);
      ClearStatistics();
    }

    //***********************************
    void ClearStatistics()
    {
      startCount    = 0;
      stopCount     = 0;
      setSpeedCount = 0;
      unknownCount  = 0;
      stoppedCount  = 0;
      exited_state  = false;
      entered_state = false;
      isLampOn      = false;
      speed         = 0;

      last_event_id          = std::numeric_limits<etl::message_id_t>::max();
      last_returned_state_id = std::numeric_limits<etl::fsm_state_id_t>::max();
    }

    //***********************************
    void SetSpeedValue(int speed_)
    {
      speed = speed_;
    }

    //***********************************
    void TurnRunningLampOn()
    {
      isLampOn = true;
    }

    //***********************************
    void TurnRunningLampOff()
    {
      isLampOn = false;
    }

    //***********************************
    template <typename T>
    void queue_recursive_message(const T& message)
    {
      messageQueue.emplace(message);
    }

    typedef etl::largest<Start, Stop, SetSpeed, Stopped, Recursive, SelfTransition> Largest_t;

    typedef etl::packet<etl::imessage, Largest_t::size, Largest_t::alignment> Packet_t;

    etl::queue<Packet_t, 2> messageQueue;

    int  startCount;
    int  stopCount;
    int  setSpeedCount;
    int  unknownCount;
    int  stoppedCount;
    bool exited_state;
    bool entered_state;
    bool isLampOn;
    int  speed;
    etl::message_id_t   last_event_id;
    etl::fsm_state_id_t last_returned_state_id;
  };

  //***********************************
  // The idle state.
  //***********************************
  class Idle : public etl::fsm_state<MotorControl, Idle, StateId::Idle, Start, Recursive, SelfTransition>
  {
  public:

    //***********************************
    etl::fsm_state_id_t on_event(const Start&)
    {
      ++get_fsm_context().startCount;
      return StateId::Running;
    }

    //***********************************
    etl::fsm_state_id_t on_event(const Recursive&)
    {
      get_fsm_context().queue_recursive_message(Start());
      return StateId::Idle;
    }

    //***********************************
    etl::fsm_state_id_t on_event(const SelfTransition&)
    {
      get_fsm_context().last_event_id          = SelfTransition::ID;
      get_fsm_context().last_returned_state_id = etl::ifsm_state::Self_Transition;
      return etl::ifsm_state::Self_Transition;
    }

    //***********************************
    etl::fsm_state_id_t on_event_unknown(const etl::imessage&)
    {
      ++get_fsm_context().unknownCount;
      return StateId::Idle; //No_State_Change;
    }

    //***********************************
    etl::fsm_state_id_t on_enter_state()
    {
      get_fsm_context().TurnRunningLampOff();
      get_fsm_context().entered_state = true;
      return StateId::Locked;
    }

    //***********************************
    void on_exit_state()
    {
      get_fsm_context().exited_state = true;
    }
  };

  //***********************************
  // The running state.
  //***********************************
  class Running : public etl::fsm_state<MotorControl, Running, StateId::Running, Stop, SetSpeed>
  {
  public:

    //***********************************
    etl::fsm_state_id_t on_event(const Stop& event)
    {
      ++get_fsm_context().stopCount;

      if (event.isEmergencyStop)
      {
        return StateId::Idle;
      }
      else
      {
        return StateId::Winding_Down;
      }
    }

    //***********************************
    etl::fsm_state_id_t on_event(const SetSpeed& event)
    {
      ++get_fsm_context().setSpeedCount;
      get_fsm_context().SetSpeedValue(event.speed);
      return No_State_Change;
    }

    //***********************************
    etl::fsm_state_id_t on_event_unknown(const etl::imessage&)
    {
      ++get_fsm_context().unknownCount;
      return No_State_Change;
    }

    //***********************************
    etl::fsm_state_id_t on_enter_state()
    {
      get_fsm_context().TurnRunningLampOn();

      return No_State_Change;
    }
  };

  //***********************************
  // The winding down state.
  //***********************************
  class WindingDown : public etl::fsm_state<MotorControl, WindingDown, StateId::Winding_Down, Stopped>
  {
  public:

    //***********************************
    etl::fsm_state_id_t on_event(const Stopped&)
    {
      ++get_fsm_context().stoppedCount;
      return StateId::Idle;
    }

    //***********************************
    etl::fsm_state_id_t on_event_unknown(const etl::imessage&)
    {
      ++get_fsm_context().unknownCount;
      return No_State_Change;
    }
  };

  //***********************************
  // The locked state.
  //***********************************
  class Locked : public etl::fsm_state<MotorControl, Locked, StateId::Locked>
  {
  public:

    //***********************************
    etl::fsm_state_id_t on_event_unknown(const etl::imessage&)
    {
      ++get_fsm_context().unknownCount;
      return No_State_Change;
    }
  };

  // The states.
  Idle        idle;
  Running     running;
  WindingDown windingDown;
  Locked      locked;

  etl::ifsm_state* stateList[StateId::Number_Of_States] =
  {
    &idle, &running, &windingDown, &locked
  };

  MotorControl motorControl;

  SUITE(test_fsm_states)
  {
    //*************************************************************************
    TEST(test_fsm)
    {
      etl::null_message_router nmr;

      CHECK(motorControl.is_producer());
      CHECK(motorControl.is_consumer());

      motorControl.Initialise(stateList, ETL_OR_STD17::size(stateList));
      motorControl.reset();
      motorControl.ClearStatistics();

      CHECK(!motorControl.is_started());

      // Start the FSM.
      motorControl.start(false);
      CHECK(motorControl.is_started());

      // Now in Idle state.

      CHECK_EQUAL(StateId::Idle, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Idle, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(false, motorControl.isLampOn);
      CHECK_EQUAL(0, motorControl.setSpeedCount);
      CHECK_EQUAL(0, motorControl.speed);
      CHECK_EQUAL(0, motorControl.startCount);
      CHECK_EQUAL(0, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(0, motorControl.unknownCount);

      // Send unhandled events.
      motorControl.receive(Stop());
      motorControl.receive(Stopped());
      motorControl.receive(SetSpeed(10));

      CHECK_EQUAL(StateId::Idle, motorControl.get_state_id());
      CHECK_EQUAL(StateId::Idle, motorControl.get_state().get_state_id());

      CHECK_EQUAL(false, motorControl.isLampOn);
      CHECK_EQUAL(0, motorControl.setSpeedCount);
      CHECK_EQUAL(0, motorControl.speed);
      CHECK_EQUAL(0, motorControl.startCount);
      CHECK_EQUAL(0, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(3, motorControl.unknownCount);

      // Send Start event.
      motorControl.receive(Start());

      // Now in Running state.

      CHECK_EQUAL(StateId::Running, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Running, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(true, motorControl.isLampOn);
      CHECK_EQUAL(0, motorControl.setSpeedCount);
      CHECK_EQUAL(0, motorControl.speed);
      CHECK_EQUAL(1, motorControl.startCount);
      CHECK_EQUAL(0, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(3, motorControl.unknownCount);

      // Send unhandled events.
      motorControl.receive(Start());
      motorControl.receive(Stopped());

      CHECK_EQUAL(StateId::Running, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Running, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(true, motorControl.isLampOn);
      CHECK_EQUAL(0, motorControl.setSpeedCount);
      CHECK_EQUAL(0, motorControl.speed);
      CHECK_EQUAL(1, motorControl.startCount);
      CHECK_EQUAL(0, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(5, motorControl.unknownCount);

      // Send SetSpeed event.
      motorControl.receive(SetSpeed(100));

      // Still in Running state.

      CHECK_EQUAL(StateId::Running, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Running, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(true, motorControl.isLampOn);
      CHECK_EQUAL(1, motorControl.setSpeedCount);
      CHECK_EQUAL(100, motorControl.speed);
      CHECK_EQUAL(1, motorControl.startCount);
      CHECK_EQUAL(0, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(5, motorControl.unknownCount);

      // Send Stop event.
      motorControl.receive(Stop());

      // Now in WindingDown state.

      CHECK_EQUAL(StateId::Winding_Down, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Winding_Down, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(true, motorControl.isLampOn);
      CHECK_EQUAL(1, motorControl.setSpeedCount);
      CHECK_EQUAL(100, motorControl.speed);
      CHECK_EQUAL(1, motorControl.startCount);
      CHECK_EQUAL(1, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(5, motorControl.unknownCount);

      // Send unhandled events.
      motorControl.receive(Start());
      motorControl.receive(Stop());
      motorControl.receive(SetSpeed(100));

      CHECK_EQUAL(StateId::Winding_Down, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Winding_Down, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(true, motorControl.isLampOn);
      CHECK_EQUAL(1, motorControl.setSpeedCount);
      CHECK_EQUAL(100, motorControl.speed);
      CHECK_EQUAL(1, motorControl.startCount);
      CHECK_EQUAL(1, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(8, motorControl.unknownCount);

      // Send Stopped event.
      motorControl.receive(Stopped());

      // Now in Locked state via Idle state.
      CHECK_EQUAL(StateId::Locked, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Locked, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(false, motorControl.isLampOn);
      CHECK_EQUAL(1, motorControl.setSpeedCount);
      CHECK_EQUAL(100, motorControl.speed);
      CHECK_EQUAL(1, motorControl.startCount);
      CHECK_EQUAL(1, motorControl.stopCount);
      CHECK_EQUAL(1, motorControl.stoppedCount);
      CHECK_EQUAL(8, motorControl.unknownCount);
    }

    //*************************************************************************
    TEST(test_fsm_emergency_stop)
    {
      etl::null_message_router nmr;

      motorControl.Initialise(stateList, ETL_OR_STD17::size(stateList)); 
      motorControl.reset();
      motorControl.ClearStatistics();

      CHECK(!motorControl.is_started());

      // Start the FSM.
      motorControl.start(false);
      CHECK(motorControl.is_started());

      // Now in Idle state.

      // Send Start event.
      motorControl.receive(Start());

      // Now in Running state.

      CHECK_EQUAL(StateId::Running, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Running, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(true, motorControl.isLampOn);
      CHECK_EQUAL(0, motorControl.setSpeedCount);
      CHECK_EQUAL(0, motorControl.speed);
      CHECK_EQUAL(1, motorControl.startCount);
      CHECK_EQUAL(0, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(0, motorControl.unknownCount);

      // Send emergency Stop event.
      motorControl.receive(Stop(true));

      // Now in Locked state via Idle state.
      CHECK_EQUAL(StateId::Locked, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Locked, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(false, motorControl.isLampOn);
      CHECK_EQUAL(0, motorControl.setSpeedCount);
      CHECK_EQUAL(0, motorControl.speed);
      CHECK_EQUAL(1, motorControl.startCount);
      CHECK_EQUAL(1, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(0, motorControl.unknownCount);
    }

    //*************************************************************************
    TEST(test_fsm_recursive_event)
    {
      etl::null_message_router nmr;

      motorControl.Initialise(stateList, ETL_OR_STD17::size(stateList));
      motorControl.reset();
      motorControl.ClearStatistics();

      motorControl.messageQueue.clear();

      // Start the FSM.
      motorControl.start(false);

      // Now in Idle state.
      // Send Start event.
      motorControl.receive(Recursive());

      CHECK_EQUAL(1U, motorControl.messageQueue.size());

      // Send the queued message.
      motorControl.receive(motorControl.messageQueue.front().get());
      motorControl.messageQueue.pop();

      // Now in Running state.

      CHECK_EQUAL(StateId::Running, int(motorControl.get_state_id()));
      CHECK_EQUAL(StateId::Running, int(motorControl.get_state().get_state_id()));

      CHECK_EQUAL(true, motorControl.isLampOn);
      CHECK_EQUAL(0, motorControl.setSpeedCount);
      CHECK_EQUAL(0, motorControl.speed);
      CHECK_EQUAL(1, motorControl.startCount);
      CHECK_EQUAL(0, motorControl.stopCount);
      CHECK_EQUAL(0, motorControl.stoppedCount);
      CHECK_EQUAL(0, motorControl.unknownCount);
    }

    //*************************************************************************
    TEST(test_fsm_supported)
    {
      CHECK(motorControl.accepts(EventId::Set_Speed));
      CHECK(motorControl.accepts(EventId::Start));
      CHECK(motorControl.accepts(EventId::Stop));
      CHECK(motorControl.accepts(EventId::Stopped));
      CHECK(motorControl.accepts(EventId::Unsupported));

      CHECK(motorControl.accepts(SetSpeed(0)));
      CHECK(motorControl.accepts(Start()));
      CHECK(motorControl.accepts(Stop()));
      CHECK(motorControl.accepts(Stopped()));
      CHECK(motorControl.accepts(Unsupported()));
    }

    //*************************************************************************
    TEST(test_fsm_no_states)
    {
      MotorControl mc;

      // No states.
      etl::ifsm_state** stateList = nullptr;

      CHECK_THROW(mc.set_states(stateList, 0U), etl::fsm_state_list_exception);
    }

    //*************************************************************************
    TEST(test_fsm_null_state)
    {
      MotorControl mc;

      // Null state.
      etl::ifsm_state* stateList[StateId::Number_Of_States] =
      {
        &idle, &running,& windingDown, nullptr
      };

      CHECK_THROW(mc.set_states(stateList, StateId::Number_Of_States), etl::fsm_null_state_exception);
    }

    //*************************************************************************
    TEST(test_fsm_incorrect_state_order)
    {
      MotorControl mc;

      // Incorrect order.
      etl::ifsm_state* stateList[StateId::Number_Of_States] =
      {
        &idle, &windingDown, &running, &locked
      };

      CHECK_THROW(mc.set_states(stateList, StateId::Number_Of_States), etl::fsm_state_list_order_exception);
    }

    //*************************************************************************
    TEST(test_fsm_self_transition)
    {
      MotorControl motorControl;

      motorControl.Initialise(stateList, ETL_OR_STD17::size(stateList));
      motorControl.reset();
      motorControl.ClearStatistics();

      // Start the FSM.
      motorControl.start(false);

      CHECK_FALSE(motorControl.exited_state);
      CHECK_FALSE(motorControl.entered_state);

      // Execute self transition.
      motorControl.receive(SelfTransition());

      CHECK_EQUAL(size_t(SelfTransition::ID),               size_t(motorControl.last_event_id));
      CHECK_EQUAL(size_t(etl::ifsm_state::Self_Transition), size_t(motorControl.last_returned_state_id));

      CHECK_TRUE(motorControl.exited_state);
      CHECK_TRUE(motorControl.entered_state);
    }

    //*************************************************************************
    TEST(test_fsm_no_states_and_no_start)
    {
      MotorControl mc;

      CHECK_THROW(mc.receive(Start()), etl::fsm_not_started);
    }
  };
}
