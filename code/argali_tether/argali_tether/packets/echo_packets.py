from .packet_base import PacketBase, PacketFieldTypes, PacketField

class EchoRequestPacket(PacketBase):
    PACKET_FAMILY = 'E'
    PACKET_TYPE = 'Q'

    @classmethod
    def fields(cls):
        return [
            PacketField("content", PacketFieldTypes.BYTES,  None),
            ]


class EchoReplyPacket(EchoRequestPacket):
    PACKET_TYPE = 'R'


class EchoTableReplyPacket(EchoRequestPacket):
    PACKET_TYPE = 'T'
    @classmethod
    def fields(cls):
        return [
            ]


class EchoTableReplyPacket(EchoRequestPacket):
    PACKET_TYPE = 'U'
    @classmethod
    def fields(cls):
        return [
            PacketField("content", PacketFieldTypes.BYTES,  256),
            ]
