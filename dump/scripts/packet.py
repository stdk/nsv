from struct import pack,unpack,calcsize

PACKET_FORMAT = 'IIIII8s'
PACKET_LENGTH = calcsize(PACKET_FORMAT)

ID_DUMP_FORMAT = 'addr[%04X] func[%02X] num[%X] end[%X]'
PACKET_DUMP_FORMAT = 'flags[%X] id[%s] length[%X] data[%s]'

FLAG_EXT_ID         = 0x20
FLAG_CONTROL        = 0x80
CAN_MASTER_ADDR = 0x8000
CAN_MSG_LENGTH = 8

null_byte_str = ''.join(['\0' for i in range(CAN_MSG_LENGTH)])

class Packet(object):
 @staticmethod
 def Control(flags):
  return Packet(flags = FLAG_CONTROL | flags) 
 @staticmethod
 def Command(addr=CAN_MASTER_ADDR,func=0,num=0,end=1,data=''):
  return Packet(addr,func,num,end,data,FLAG_EXT_ID)
 @staticmethod
 def Parse(packet_bytes):
  flags,id,t_s,t_ms,length,data = unpack(PACKET_FORMAT,packet_bytes)
  end = id & 0x01
  num = (id >> 1) & 0x0F
  func = (id >> (1 + 4)) & 0xFF
  addr = (id >> (1 + 4 + 8)) & 0xFFFF
  return Packet(addr,func,num,end,data,flags,length)  
 
 def __init__(self,addr=0,func=0,num=0,end=1,data='',flags=FLAG_EXT_ID,length=None):
  self.addr = addr
  self.func = func
  self.num = num
  self.end = end
  self.set_data(data,length)
  self.flags = flags
 
 def set_data(self,data,length=None):
  if length == None:
   self.length = len(data)
   if self.length > 8: self.length = 8
   if self.length < 8: data += null_byte_str[:(CAN_MSG_LENGTH-self.length)]
  else:
   self.length = length
  self.data = data
 
 def id(self):
  return (self.end & 0x01) | ((self.num & 0x0F) << 1) | ((self.func & 0xFF) << (1 + 4)) | ((self.addr & 0xFFFF) << (1 + 4 + 8))
 def bytes(self):
  return pack(PACKET_FORMAT, self.flags,self.id(),0,0,self.length,self.data)
  
 def __str__(self):
  id_str = ID_DUMP_FORMAT % (self.addr,self.func,self.num,self.end)
  data_str = ' '.join(['%02X' % ord(i) for i in self.data])
  return PACKET_DUMP_FORMAT % (self.flags,id_str,self.length,data_str)
  
 def send(self,sock):
  return sock.send(self.bytes())
 @staticmethod
 def recv(sock):
  packet_bytes = sock.recv(PACKET_LENGTH)
  return Packet.Parse(packet_bytes)
 

 