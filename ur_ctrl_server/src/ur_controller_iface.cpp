
#include "ur_ctrl_server/ur_controller_iface.h"
#include "stdlib.h"
#include "stdio.h"

namespace ur {

URControllerInterface::URControllerInterface(SimpleSocket* socket_conn) :
  latest_cmd_seq(-10000),
  //was_emergency_stopped(false)
  connection(socket_conn)
{
  msg_man.init(connection);

  joint_cmd_handler.init(ur::URMessageTypes::JOINT_CMD, connection, &jnt_cmd);
  msg_man.add(&joint_cmd_handler);

  config_cmd_handler.init(ur::URMessageTypes::CONFIG_CMD, connection, &config_cmd);
  msg_man.add(&config_cmd_handler);
}

URControllerInterface::~URControllerInterface() {}

void URControllerInterface::sendAndReceiveMessages()
{
  if(!connection->isConnected() && connection->isReadyReceive(0)) {
    // only attempt to connect if it will not block for long
    connection->makeConnect(); // should block until a handshake occurs
    latest_cmd_seq = ur_state.sequence + STARTUP_TIMEOUT_BIAS;
  }
  if(!connection->isConnected()) 
    return;

  // send state messages
  ur_state.toSimpleMessage(ur::URMessageTypes::STATE, ur_state_simp_msg);
  connection->sendMsg(ur_state_simp_msg);

  // reset incoming message handlers
  joint_cmd_handler.reset();
  config_cmd_handler.reset();
  
  // process all incoming messages
  while(connection->isReadyReceive(0))
    msg_man.spinOnce();

  if(joint_cmd_handler.hasUpdated()) 
    latest_cmd_seq = ur_state.sequence;

  if(ur_state.sequence - latest_cmd_seq >= CMD_TIMEOUT) {
    // printf("TIMEOUT\n");
    connection->setDisconnected();
  }
}

void URControllerInterface::controlLoop()
{
  while(1) {
    readRobotState();
    sendAndReceiveMessages();
    sendRobotCommands();
    ur_state.sequence++;
  }
}

}
