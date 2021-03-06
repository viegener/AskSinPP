
#ifndef __MULTICHANNELDEVICE_H__
#define __MULTICHANNELDEVICE_H__

#include <Device.h>
#include <Defines.h>
#include <cm.h>
#include <Led.h>
#include <Activity.h>

namespace as {

template <class ChannelType,int ChannelCount,class List0Type=List0>
class MultiChannelDevice : public Device {

  List0Type   list0;
  ChannelType devchannels[ChannelCount];
  uint8_t     cfgChannel;
  GenericList cfgList;

public:
  MultiChannelDevice (uint16_t addr) : list0(addr), cfgChannel(0xff) {
    addr += list0.size();
    for( uint8_t i=0; i<channels(); ++i ) {
      devchannels[i].setup(this,i+1,addr);
      addr += devchannels[i].size();
    }
  }
  virtual ~MultiChannelDevice () {}


  uint8_t channels () const {
    return ChannelCount;
  }

  bool hasChannel (uint8_t number) const {
    return number != 0 && number <= ChannelCount;
  }

  void init (CC1101& r,const HMID& id,const char* serial) {
    // read master id from flash
    setMasterID(list0.masterid());
    setDeviceID(id);
    setSerial(serial);
    setRadio(r);
  }

  void init (CC1101& r,uint16_t idaddr, uint16_t serialaddr) {
    // read master id from flash
    setMasterID(list0.masterid());
    // read id & serial from bootloader
    setDeviceID(idaddr);
    setSerial(serialaddr);
    setRadio(r);
  }

  void firstinit () {
    list0.defaults();
    for( uint8_t i=0; i<channels(); ++i ) {
      devchannels[i].firstinit();
    }
  }

  void reset () {
    firstinit();
    setMasterID(list0.masterid());
    sled.set(StatusLed::welcome);
  }

  void startPairing () {
    sled.set(StatusLed::pairing);
    activity.stayAwake( seconds2ticks(20) ); // 20 seconds
    sendDeviceInfo();
  }

  ChannelType& channel(uint8_t ch) {
    return devchannels[ch-1];
  }

  bool pollRadio () {
    bool worked = Device::pollRadio();
    for( uint8_t i=1; i<=channels(); ++i ) {
      ChannelType& ch = channel(i);
      if( ch.changed() == true ) {
        sendInfoActuatorStatus(getMasterID(),nextcount(),ch);
        worked = true;
      }
    }
    return worked;
  }

   void process(Message& msg) {
     if( msg.to() == getDeviceID() || (msg.to() == HMID::boardcast && isBoardcastMsg(msg))) {
       DPRINT(F("-> "));
       msg.dump();
       if( msg.type() == AS_MESSAGE_CONFIG ) {
         // PAIR_SERIAL
         if( msg.subcommand() == AS_CONFIG_PAIR_SERIAL && memcmp(msg.data(),getSerial(),10)==0 ) {
           sled.set(StatusLed::pairing);
           list0.masterid(msg.from());
           setMasterID(list0.masterid());
           sendDeviceInfo(getMasterID(),msg.count());
         }
         // CONFIG_PEER_ADD
         else if ( msg.subcommand() == AS_CONFIG_PEER_ADD ) {
           const ConfigPeerAddMsg& pm = msg.configPeerAdd();
           bool success = false;
           if( hasChannel(pm.channel()) == true ) {
             ChannelType& ch = channel(pm.channel());
             if( pm.peers() == 1 ) {
               success = addPeer(ch,pm.peer1());
             }
             else {
               success = addPeer(ch,pm.peer1(),pm.peer2());
             }
           }
           success == true ? sendAck(msg) : sendNack(msg);
         }
         // CONFIG_PEER_REMOVE
         else if ( msg.subcommand() == AS_CONFIG_PEER_REMOVE ) {
           const ConfigPeerRemoveMsg& pm = msg.configPeerRemove();
           bool success = false;
           if( hasChannel(pm.channel()) == true ) {
             ChannelType& ch = channel(pm.channel());
             success = ch.deletepeer(pm.peer1());
             if( pm.peers() == 2 ) {
               success &= ch.deletepeer(pm.peer2());
             }
           }
           success == true ? sendAck(msg) : sendNack(msg);
         }
         // CONFIG_PEER_LIST_REQ
         else if( msg.subcommand() == AS_CONFIG_PEER_LIST_REQ ) {
           const ConfigPeerListReqMsg& pm = msg.configPeerListReq();
           if( hasChannel(pm.channel()) == true ) {
             sendInfoPeerList(msg.from(),msg.count(),channel(pm.channel()));
           }
         }
         // CONFIG_PARAM_REQ
         else if (msg.subcommand() == AS_CONFIG_PARAM_REQ ) {
           const ConfigParamReqMsg& pm = msg.configParamReq();
           GenericList gl = findList(pm.channel(),pm.peer(),pm.list());
           if( gl.valid() == true ) {
             sendInfoParamResponsePairs(msg.from(),msg.count(),gl);
           }
         }
         // CONFIG_STATUS_REQUEST
         else if (msg.subcommand() == AS_CONFIG_STATUS_REQUEST ) {
           sendInfoActuatorStatus(msg.from(),msg.count(),channel(msg.command()));
         }
         // CONFIG_START
         else if( msg.subcommand() == AS_CONFIG_START ) {
           const ConfigStartMsg& pm = msg.configStart();
           cfgChannel = pm.channel();
           cfgList = findList(cfgChannel,pm.peer(),pm.list());
           // TODO setup alarm to disable after 2000ms
           sendAck(msg);
         }
         // CONFIG_END
         else if( msg.subcommand() == AS_CONFIG_END ) {
           if( cfgList.address() == list0.address() ) {
             setMasterID(list0.masterid());
             sled.set(StatusLed::nothing);
           }
           cfgChannel = 0xff;
           // TODO cancel alarm
           sendAck(msg);
         }
         else if( msg.subcommand() == AS_CONFIG_WRITE_INDEX ) {
           const ConfigWriteIndexMsg& pm = msg.configWriteIndex();
           if( cfgChannel == pm.channel() && cfgList.valid() == true ) {
             writeList(cfgList,pm.data(),pm.datasize());
           }
           sendAck(msg);
         }
         // default - send Ack if requested
         else {
           if( msg.ackRequired() == true ) {
             sendAck(msg);
           }
         }
       }
       else if( msg.type() == AS_MESSAGE_ACTION ) {
         if( msg.command() == AS_ACTION_SET ) {
           const ActionSetMsg& pm = msg.action();
           ChannelType& c = channel(pm.channel());
           c.process(pm);
           sendAck(msg,c);
         }
       }
       else if (msg.type() == AS_MESSAGE_REMOTE_EVENT ) {
         const RemoteEventMsg& pm = msg.remoteEvent();
         bool found=false;
//         p.dump();
         for( uint8_t i=1; i<=channels(); ++i ) {
           ChannelType& ch = channel(i);
           if( ch.process(pm) == true ) {
             sendAck(msg,ch);
             found=true;
           }
         }
         if( found==false ) {
           sendNack(msg);
         }
       }
       // default - send Ack if requested
       else {
         if( msg.ackRequired() == true ) {
           sendAck(msg);
         }
       }
     }
     else {
//       DPRINT(F("ignore "));
//       msg.dump();
     }
   }

   GenericList findList(uint8_t ch,const Peer& peer,uint8_t numlist) {
    if (numlist == 0) {
      return list0;
    } else if (hasChannel(ch) == true) {
      ChannelType& c = channel(ch);
      if (numlist == 1) {
        return c.getList1();
      } else if (ChannelType::hasList3() && numlist == 3) {
        return c.getList3(peer);
      } else if (ChannelType::hasList4() && numlist == 4) {
        return c.getList4(peer);
      }
    }
    return GenericList();
   }

};

}

#endif
