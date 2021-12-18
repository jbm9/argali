from .packet_base import PacketBase, PacketFieldTypes, PacketField

class ADCConfigPacket(PacketBase):
    PACKET_FAMILY = 'A'
    PACKET_TYPE = 'C'

    @classmethod
    def fields(cls):
        return [
            PacketField("prescaler", PacketFieldTypes.UINT16_T),
            PacketField("period", PacketFieldTypes.UINT32_T),
            PacketField("num_points", PacketFieldTypes.UINT16_T),
            PacketField("sample_width", PacketFieldTypes.UINT8_T),
            PacketField("sample_time", PacketFieldTypes.UINT16_T),
            PacketField("channels", PacketFieldTypes.UINT8_T,
                        length=None,
                        lengthtype=PacketFieldTypes.UINT8_T),
            ]
    
