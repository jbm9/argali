from .packet_base import PacketBase, PacketFieldTypes, PacketField

class DACConfigPacket(PacketBase):
    '''A packet to request configuration of the DAC, without starting it


    This populates the DAC with a single sine wave to play back using
    the given parameters.

    To start the DAC up, use a DACStartPacket, then a DACStopPacket to
    stop it.
    '''
    PACKET_FAMILY = 'D'
    PACKET_TYPE = 'C'

    @classmethod
    def fields(cls):
        return [
            PacketField("prescaler", PacketFieldTypes.UINT16_T),
            PacketField("period", PacketFieldTypes.UINT32_T),
            PacketField("scale", PacketFieldTypes.UINT8_T),
            PacketField("points_per_wave", PacketFieldTypes.UINT16_T),
            PacketField("num_waves", PacketFieldTypes.UINT8_T),
            PacketField("theta0_u8", PacketFieldTypes.UINT8_T),            
            ]

class DACConfigACKPacket(DACConfigPacket):
    PACKET_TYPE='c'

    @classmethod
    def fields(cls):
        return [
            PacketField("sample_rate", PacketFieldType.FLOAT),
            ]
    
class DACStartPacket(DACConfigPacket):
    '''Start the DAC up with its current configuration

    Note that there are no sanity checks on this at present, so it
    just blindly attempts to do whatever it's currently configured to
    do.

    There is no leash on this: it will play the buffer out until
    a DACStopPacket is received, or the micro is reset.

    '''
    PACKET_TYPE = 'S'

    @classmethod
    def fields(cls):
        return [] # No fields

class DACStartACKPacket(DACStartPacket):
    PACKET_TYPE = 's'
    
    
class DACStopPacket(DACStartPacket):
    PACKET_TYPE = 'T'
    
class DACStopACKPacket(DACStopPacket):
    PACKET_TYPE = 't'
