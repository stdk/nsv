from socket import socket,AF_INET,SOCK_STREAM
from struct import pack,unpack
import asyncore
from packet import Packet,CAN_MSG_LENGTH

FLAG_BUS_ERROR      = 1
FLAG_ERROR_PASSIVE  = 2
FLAG_DATA_OVERRUN   = 4
FLAG_ERROR_WARNING  = 8
FLAG_RTR            = 0x10
FLAG_EXT_ID         = 0x20
FLAG_LOCAL          = 0x40
FLAG_CONTROL        = 0x80
FLAG_CMD_RXEN       = 1

CAN_GET_SN           = 4
CAN_SET_FLASH_ADDR   = 6
CAN_SET_FLASH_DATA   = 7
CAN_DELAY_UPDATE_PRG = 13
CAN_GET_EVENT        = 14

CANCTL_HOST = '127.0.0.1'
CANCTL_PORT = 7552

can_client = None

def open():
 global can_client
 can_client =  CANClient(CANCTL_HOST,CANCTL_PORT)

def perform_action(action):
 #client = CANClient(CANCTL_HOST,CANCTL_PORT)
 #client.append_action(action)
 can_client.recv_storage = []
 action(can_client)
 asyncore.loop(count=60)
 #client.close()
 return can_client.recv_storage

def send_action(p):
 def action(sock):
  p.send(sock) 
 return action

def init_control_packet():
 return Packet.Control(FLAG_CMD_RXEN)
 
def get_sn_action(addr):
 command_packet = Packet.Command(addr=addr,func=CAN_GET_SN)
 return send_action(command_packet)
 
def get_event_action(addr,event_id):
 command_packet = Packet.Command(addr=addr,func=CAN_GET_EVENT,data=pack('I',event_id))
 return send_action(command_packet)

def set_flash_addr_packet(addr,flash_addr):
 return Packet.Command(addr=addr,func=CAN_SET_FLASH_ADDR,data=pack('I',flash_addr))
 
def set_flash_addr_action(addr,flash_addr):
 command_packet = Packet.Command(addr=addr,func=CAN_SET_FLASH_ADDR,data=pack('I',flash_addr))
 return send_action(command_packet)

def delayed_update_packet(addr):
 return Packet.Command(addr=addr,func=CAN_DELAY_UPDATE_PRG)
 
def delayed_update_action(addr):
 command_packet = Packet.Command(addr=addr,func=CAN_DELAY_UPDATE_PRG)
 return send_action(command_packet)
 
def set_flash_data_packet_generator(addr,data):
 length = len(data)
 if not length: raise 'Not enough data for write_flash_data_action'
 packet_chain_length = (length / CAN_MSG_LENGTH) + (1 if length % CAN_MSG_LENGTH else 0)
 def packet_generator():
  for i in range(packet_chain_length):
   bound_left,bound_right = i*CAN_MSG_LENGTH, i*CAN_MSG_LENGTH + CAN_MSG_LENGTH
   is_end = (i == (packet_chain_length-1))
   yield Packet.Command(addr=addr,func=CAN_SET_FLASH_DATA,num=i,end=is_end,data=data[bound_left:bound_right])
 return packet_generator
 
def receive_packet(sock):
 return Packet.recv(sock)
 
class CANClient(asyncore.dispatcher):
 def __init__(self, host, port):
  asyncore.dispatcher.__init__(self)
  self.create_socket(AF_INET, SOCK_STREAM)
  self.connect( (host, port) )		
  Packet.Control(FLAG_CMD_RXEN).send(self)
  self.recv_storage = []
 
 def append_action(self,action):
  self.actions.append(action) 
  
 def handle_close(self):
  self.close()

 def handle_read(self):
  pack = Packet.recv(self)
  self.recv_storage.append(pack)