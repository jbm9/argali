from enum import Enum
import struct

from abc import ABC

class PacketFieldTypes(Enum):
    UINT8_T = 'B'
    INT8_T = 'b'
    UINT16_T = 'H'
    INT16_T = 'h'
    UINT32_T = 'L'
    INT32_T = 'l'
    FLOAT = 'f'
    DOUBLE = 'd'
    BYTES = 's'

class PacketField:
    def __init__(self, fname, ftype, length=1, lengthtype=None):
        '''Describes a single field in a packet

        ftype is from PacketFieldTypes

        fname is the human name for the value, which should correspond
        1:1 with a property of the class for this packet

        length is the number of entries of that type in the value.  If
        this is None, it's treated as a variable length value.  This
        will autogenerate a uint16_t length parameter before your
        value. Fixed-length fields do not get that length value.

        lengthtype is the type to use for length fields of variable
        length fields.  The default is uint16_t, but you can override
        this with an integer PacketFieldType and that will be used
        instead.

        '''
        self.ftype = ftype
        self.fname = fname
        self.length = length
        self.lengthtype = lengthtype

        if self.length is not None and self.length < 1:
            raise ValueError(f'Tried to assign a non-positive length value for {self.fname}: {self.length}')

        self.is_varlength = (self.length is None)

    def pack_str(self, value):
        '''Internal: get the struct.pack() string for this field'''

        length_ft = self.lengthtype
        if length_ft is None:
            length_ft = PacketFieldTypes.UINT16_T

        if length_ft not in [
                PacketFieldTypes.UINT8_T,
                PacketFieldTypes.INT8_T,
                PacketFieldTypes.UINT16_T,
                PacketFieldTypes.INT16_T,
                PacketFieldTypes.UINT32_T,
                PacketFieldTypes.INT32_T]:
            raise ValueError(F'Invalid length type: {self.lengthtype}')

        l_p = length_ft.value
        
        if self.is_varlength:
            self.length = len(value)
            if self.length == 0:
                return l_p # Only the length field needed
            return f'{l_p}{self.length}{self.ftype.value}'

        # All varlength handled above here, so fixed length
        if self.length > 1:
            return f'{l_p}{self.length}{self.ftype.value}'
        return self.ftype.value

    def pack_vals(self, value):
        '''Internal: get the value array to append for this entry

        This whole function is made of weird assumptions about types,
        due to python's loose typing.
        '''
        length = self.length
        if self.is_varlength:
            length = len(value)

        if self.ftype == PacketFieldTypes.BYTES:
            # Need to return the scalar for strings
            if self.is_varlength:
                return [length, value]
            return [value]

        # All types except "string" should return an array of N
        # values, where N is the number of entries to write out.
        # Python's struct module is pretty primitive about this, but
        # understandably so.  So, if we have a scalar, we bundle it up
        # in a list before sending it over.  Otherwise, we return a
        # list that is the optional length prefix followed by all the
        # entries in that list.
        
        result = value
        
        if value.__class__ != list:
            result = [value]

        if self.is_varlength:
            return [len(value), *result]

        return result

    def unpack_head(self, buf):
        '''Internal: returns a tuple of the value and the number of bytes extracted'''
        cursor = 0
        n = self.length
        if self.is_varlength:
            n = struct.unpack(">H", buf[cursor:cursor+2])[0]
            cursor += 2

        unpack_str = f'>{n}{self.ftype.value}'
        # print(unpack_str)        
        sz = struct.calcsize(unpack_str)
        result = struct.unpack(unpack_str, buf[cursor:cursor+sz])
        # print(n, result)
        if n == 1:
            result = result[0]
        if self.ftype == PacketFieldTypes.BYTES:
            result = bytes(result[0])
        
        return (result, cursor+sz)

    def struct_str(self):
        result = []  # list of strings we add entries to        
        ctypes = {
            PacketFieldTypes.UINT8_T: "uint8_t",
            PacketFieldTypes.INT8_T: "int8_t",
            PacketFieldTypes.UINT16_T: "uint16_t",
            PacketFieldTypes.INT16_T: "int16_t",
            PacketFieldTypes.UINT32_T: "uint32_t",
            PacketFieldTypes.INT32_T: "int32_t",
            PacketFieldTypes.FLOAT: "float",
            PacketFieldTypes.DOUBLE: "double",
            PacketFieldTypes.BYTES: "uint8_t",
        }

        ctyp = ctypes[self.ftype]
        prefix = ''
        suffix = ''

        if self.is_varlength:
            result.append(f'uint16_t n_{self.fname};')
            prefix = '*'
        elif self.length > 1:
            suffix = f'[{self.length}]'

        results.append(f'  {ctyp} {prefix}{self.fname}{suffix};')
            
        return results

    def add_argument(self, parser):
        argtypes = {
            PacketFieldTypes.UINT8_T: int,
            PacketFieldTypes.INT8_T: int, 
            PacketFieldTypes.UINT16_T: int,
            PacketFieldTypes.INT16_T: int, 
            PacketFieldTypes.UINT32_T: int,
            PacketFieldTypes.INT32_T: int,
            PacketFieldTypes.FLOAT: float,
            PacketFieldTypes.DOUBLE: float,
            PacketFieldTypes.BYTES: str
        }

        if self.ftype == PacketFieldTypes.BYTES or self.length == 1:
            parser.add_argument(f"--{self.fname}", type=argtypes[self.ftype])
        else:
            parser.add_argument(f"--{self.fname}", type=argtypes[self.ftype],
                                action="append", nargs="+")

    
class PacketBase:
    '''Abstract base class for packets'''

    PACKET_FAMILY = '\x00'
    PACKET_TYPE = '\x00'

    @classmethod
    def fields(cls):
        '''Returns a list of PacketField objects describing this packet'''
        pass

    def __init__(self, **kwargs):
        for k,v in kwargs.items():
            setattr(self, k, v)
        pass

    def pack(self):
        pack_str = ">BB"
        pack_vals = [ord(self.PACKET_FAMILY), ord(self.PACKET_TYPE)]

        for pf in self.fields():
            value = getattr(self, pf.fname)
            pack_str += pf.pack_str(value) + " "
            pack_vals.extend(pf.pack_vals(value))

        print(f'{pack_str}: {pack_vals}')
        return struct.pack(pack_str, *pack_vals)

    @classmethod
    def unpack(cls, buf):
        '''Unpack a packet found in the given buffer'''

        cursor = 0
        L = len(buf)

        result = {}

        packet_family = buf[0]
        packet_type = buf[1]
        cursor = 2
        
        for pf in cls.fields():
            if cursor >= L:
                raise ValueError("Got a partial packet!")
            val,n_consumed = pf.unpack_head(buf[cursor:])
            result[pf.fname] = val
            cursor += n_consumed

        return cls(**result)

    @classmethod
    def add_arguments(cls, parser):
        for f in cls.fields():
            f.add_argument(parser)

    @classmethod
    def from_args(cls, args):
        d = {}
        args_d = vars(args)
        for f in cls.fields():
            n = f.fname
            if f.ftype == PacketFieldTypes.BYTES:
               d[n] = args_d[n].encode("iso8859-1")
            elif f.length == 1:
                d[n] = args_d[n]
            else:
                d[n] = sum(args_d[n],[]) # flatten the lists
        return cls(**d)
