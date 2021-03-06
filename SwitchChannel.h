
#ifndef __SWITCHCHANNEL_H__
#define __SWITCHCHANNEL_H__

#include <SwitchList1.h>
#include <SwitchList3.h>
#include <SwitchStateMachine.h>

extern uint8_t SwitchPin(uint8_t nummber);

namespace as {

template <int PeerCount>
class SwitchChannel : public Channel<SwitchList1,SwitchList3,EmptyList,PeerCount>, public SwitchStateMachine {

  typedef Channel<SwitchList1,SwitchList3,EmptyList,PeerCount> BaseChannel;
  uint8_t lowact;

public:
  SwitchChannel () : BaseChannel(), lowact(false) {}
  virtual ~SwitchChannel() {}

  void lowactive (bool value) {
    lowact = value;
    switchState(state,state);
    BaseChannel::changed(false);
  }

  void setup(Device* dev,uint8_t number,uint16_t addr) {
    BaseChannel::setup(dev,number,addr);
    uint8_t p = SwitchPin(number);
    pinMode(p,OUTPUT);
    digitalWrite(p,lowact ? HIGH : LOW);
  }

  virtual void switchState(uint8_t oldstate,uint8_t newstate) {
    if( newstate == AS_CM_JT_ON ) {
      digitalWrite(SwitchPin(BaseChannel::number()),lowact ? LOW : HIGH);
    }
    else if ( newstate == AS_CM_JT_OFF ) {
      digitalWrite(SwitchPin(BaseChannel::number()),lowact ? HIGH : LOW);
    }
    BaseChannel::changed(true);
  }

  bool process (const ActionSetMsg& msg) {
    status( msg.value(), msg.delay() );
    return true;
  }

  bool process (const RemoteEventMsg& msg) {
    bool lg = msg.isLong();
    Peer p(msg.peer());
    uint8_t cnt = msg.counter();
    typename BaseChannel::List3 l3 = BaseChannel::getList3(p);
    if( l3.valid() == true ) {
      // l3.dump();
      typename BaseChannel::List3::PeerList pl = lg ? l3.lg() : l3.sh();
      // pl.dump();
      remote(pl,cnt);
      return true;
    }
    return false;
  }
};


};

#endif
