from .packet_base import PacketBase, PacketFieldTypes, PacketField

class SysResetPacket(PacketBase):
    '''Request a reset of the microcontroller'''
    PACKET_FAMILY = 'R'
    PACKET_TYPE = 'Q'

    @classmethod
    def fields(cls):
        return [
            ]
