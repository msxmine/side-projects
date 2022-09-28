from pymodbus.client.sync import ModbusTcpClient
from pymodbus.server.asynchronous import StartTcpServer
from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext
from pymodbus.transaction import ModbusRtuFramer, ModbusAsciiFramer, ModbusBinaryFramer
import paho.mqtt.client as mqtt
import datetime
import json
#import queue
import time
import collections
import threading
import logging
import sched
import datetime
import math
import struct

def tree(): return collections.defaultdict(tree)

class Rzadanie:
    def __init__(self, funkcja, parametr, adres):
        self.funkcja = funkcja
        self.parametr = parametr
        self.adres = adres
    def wykonaj(self):
        zamekMB.acquire()
        try:
            if (self.funkcja == "czekaj"):
                time.sleep(self.parametr)
            if (self.funkcja == "rejestr"):
                client.write_register(self.adres, self.parametr, unit=2)
            if (self.funkcja == "cewka"):
                client.write_coil(self.adres, self.parametr, unit=2)
            if (self.funkcja == "long"):
                upper = ( self.parametr & 0b11111111111111110000000000000000 ) >> 16
                lower = ( self.parametr & 0b00000000000000001111111111111111 )
                client.write_registers(self.adres, [lower, upper], unit=2)
        except Exception as error:
            #print(error)
            pass
        zamekMB.release()

class Roleta:
    def __init__(self, gora, dol, wykonaj, stan, rzadanie, dlugosc, pokoj, numer):
        self.cewka_gora = gora
        self.cewka_dol = dol
        self.cewka_exec = wykonaj
        self.rejestr_stan = stan
        self.rejestr_rzad = rzadanie
        self.dlugosc = dlugosc
        self.pokoj = pokoj
        self.numer = numer

        self.topic_stan = topic_prefix + "/" + pokoj + "/cover/" + numer + "/state"
        self.topic_ustw = topic_prefix + "/" + pokoj + "/cover/" + numer + "/set"
        self.topic_rzad = topic_prefix + "/" + pokoj + "/cover/" + numer + "/position"

        drzewo[pokoj]["cover"][numer] = self

    def subskrybuj(self):
        klient.subscribe(self.topic_ustw)
        klient.subscribe(self.topic_rzad)

    def aktualizuj(self, force):
        if ((stan_rejestry_stary[self.rejestr_stan] != stan_rejestry[self.rejestr_stan]) or (force)):
            konwersja_stan = int(round((float(stan_rejestry[self.rejestr_stan])/self.dlugosc)*100));
            klient.publish(self.topic_stan, konwersja_stan, retain=True)

    def wiadomosc(self, temat, zawartosc):
        if (temat == self.topic_ustw):
            aktywnagora = stan_cewki[self.cewka_gora];
            aktywnadol  = stan_cewki[self.cewka_dol];
            aktywnaexec = stan_cewki[self.cewka_exec];
            if (zawartosc == "OPEN"):
                #kolejka_zapisu.put(Rzadanie("cewka", False, self.cewka_dol));
                #kolejka_zapisu.put(Rzadanie("cewka", True, self.cewka_gora));
                Rzadanie("cewka", False, self.cewka_exec).wykonaj();
                Rzadanie("cewka", False, self.cewka_dol).wykonaj();
                if (aktywnadol == 1 or aktywnaexec == 1):
                    time.sleep(0.2);
                #time.sleep(0.05);
                Rzadanie("cewka", True, self.cewka_gora).wykonaj();
            if (zawartosc == "CLOSE"):
                #kolejka_zapisu.put(Rzadanie("cewka", False, self.cewka_gora));
                #kolejka_zapisu.put(Rzadanie("cewka", True, self.cewka_dol));
                Rzadanie("cewka", False, self.cewka_exec).wykonaj();
                Rzadanie("cewka", False, self.cewka_gora).wykonaj();
                if (aktywnagora == 1 or aktywnaexec == 1):
                    time.sleep(0.2);
                #time.sleep(0.05);
                Rzadanie("cewka", True, self.cewka_dol).wykonaj();
            if (zawartosc == "STOP"):
                #kolejka_zapisu.put(Rzadanie("cewka", False, self.cewka_dol));
                #kolejka_zapisu.put(Rzadanie("cewka", False, self.cewka_gora));
                Rzadanie("cewka", False, self.cewka_exec).wykonaj();
                Rzadanie("cewka", False, self.cewka_dol).wykonaj();
                Rzadanie("cewka", False, self.cewka_gora).wykonaj();
        if (temat == self.topic_rzad):
            aktywnagora = stan_cewki[self.cewka_gora];
            aktywnadol  = stan_cewki[self.cewka_dol];
            aktywnaexec = stan_cewki[self.cewka_exec];
            try:
                konwersja = int(round((float(zawartosc)/100)*self.dlugosc))
                #kolejka_zapisu.put(Rzadanie("cewka", False, self.cewka_exec));
                #kolejka_zapisu.put(Rzadanie("rejestr", konwersja, self.rejestr_rzad));
                #kolejka_zapisu.put(Rzadanie("cewka", True, self.cewka_exec));
                Rzadanie("cewka", False, self.cewka_exec).wykonaj();
                Rzadanie("cewka", False, self.cewka_dol).wykonaj();
                Rzadanie("cewka", False, self.cewka_gora).wykonaj();
                if (aktywnagora == 1 or aktywnadol == 1 or aktywnaexec == 1):
                    time.sleep(0.2);
                #time.sleep(0.05);
                Rzadanie("rejestr", konwersja, self.rejestr_rzad).wykonaj();
                Rzadanie("cewka", True, self.cewka_exec).wykonaj();
            except:
                pass

class Swiatlo:
    def __init__(self, cewka, pokoj, numer):
        self.cewka = cewka
        self.pokoj = pokoj
        self.numer = numer

        self.topic_stan = topic_prefix + "/" + pokoj + "/light/" + numer + "/state"
        self.topic_ustw = topic_prefix + "/" + pokoj + "/light/" + numer + "/set"

        drzewo[pokoj]["light"][numer] = self

    def subskrybuj(self):
        klient.subscribe(self.topic_ustw)

    def aktualizuj(self, force):
        if ((stan_cewki_stary[self.cewka] != stan_cewki[self.cewka]) or (force)):
            if stan_cewki[self.cewka] == 0:
                wiadomosc = "OFF"
            else:
                wiadomosc = "ON"
            klient.publish(self.topic_stan, wiadomosc, retain=True);

    def wiadomosc(self, temat, zawartosc):
        if (temat == self.topic_ustw):
            if (zawartosc == "OFF"):
                #kolejka_zapisu.put(Rzadanie("cewka", False, self.cewka))
                Rzadanie("cewka", False, self.cewka).wykonaj();
            if (zawartosc == "ON"):
                #kolejka_zapisu.put(Rzadanie("cewka", True, self.cewka))
                Rzadanie("cewka", True, self.cewka).wykonaj();

class StrefaPodlewania:
    def __init__(self, rejestrcurr, rejestrset, cewkaset, pokoj, numer):
        self.rejestrcurr = rejestrcurr
        self.rejestrset = rejestrset
        self.cewkaset = cewkaset

        self.topic_pozostalo = topic_prefix + "/" + pokoj + "/irrigation/" + numer + "/remaining"
        self.topic_ustaw = topic_prefix + "/" + pokoj + "/irrigation/" + numer + "/set"

        drzewo[pokoj]["irrigation"][numer] = self

    def subskrybuj(self):
        klient.subscribe(self.topic_ustaw)
    def aktualizuj(self, force):
        if ((stan_rejestry_stary[self.rejestrcurr] != stan_rejestry[self.rejestrcurr]) or (force)):
            klient.publish(self.topic_pozostalo, math.ceil( stan_rejestry[self.rejestrcurr] / 6000 ), retain=True);
    def wiadomosc(self, temat, zawartosc):
        if (temat == self.topic_ustaw):
            wartosc = int(float(zawartosc))
            if (wartosc <= 300):
                rejestr = wartosc * 6000
                Rzadanie("long", rejestr, self.rejestrset).wykonaj();
                Rzadanie("cewka", True, self.cewkaset).wykonaj();


class Czujnik:
    def __init__(self, rejestr, pokoj, numer):
        self.rejestr = rejestr
        self.pokoj = pokoj
        self.numer = numer

        self.topic_stan = topic_prefix + "/" + pokoj + "/sensor/" + numer + "/state"

        drzewo[pokoj]["sensor"][numer] = self

    def subskrybuj(self):
        pass

    def aktualizuj(self, force):
        if ((stan_rejestry_stary[self.rejestr] != stan_rejestry[self.rejestr]) or (force)):
            try:
               realval = struct.unpack('!h', struct.pack('!H', stan_rejestry[self.rejestr]))[0]
               klient.publish(self.topic_stan, realval, retain=True)
            except Exception as e:
               print(e)

    def wiadomosc(self, temat, zawartosc):
        pass

class Guzik:
    def __init__(self, cewka, cewkadluga, pokoj, numer):
        self.cewka = cewka
        self.cewkadluga = cewkadluga
        self.pokoj = pokoj
        self.numer = numer

        self.topic_stan = topic_prefix + "/" + pokoj + "/button/" + numer + "/state"

        drzewo[pokoj]["button"][numer] = self

    def subskrybuj(self):
        pass

    def aktualizuj(self, force):
        if (((stan_cewki_stary[self.cewka] == 0) or force) and (stan_cewki[self.cewka] == 1)):
            klient.publish(self.topic_stan, "ON", retain=True)
        if (((stan_cewki_stary[self.cewka] == 1) or force) and (stan_cewki[self.cewka] == 0)):
            klient.publish(self.topic_stan, "OFF", retain=True)

    def wiadomosc(self, temat, zawartosc):
        pass

class Przelacznik:
    def __init__(self, cewka, pokoj, numer):
        self.cewka = cewka
        self.pokoj = pokoj
        self.numer = numer

        self.topic_stan = topic_prefix + "/" + pokoj + "/switch/" + numer + "/state"
        self.topic_ustw = topic_prefix + "/" + pokoj + "/switch/" + numer + "/set"

        drzewo[pokoj]["switch"][numer] = self

    def subskrybuj(self):
        klient.subscribe(self.topic_ustw)

    def aktualizuj(self, force):
        if ((stan_cewki_stary[self.cewka] != stan_cewki[self.cewka]) or (force)):
            if stan_cewki[self.cewka] == 0:
                wiadomosc = "OFF"
            else:
                wiadomosc = "ON"
            klient.publish(self.topic_stan, wiadomosc, retain=True);

    def wiadomosc(self, temat, zawartosc):
        if (temat == self.topic_ustw):
            if (zawartosc == "OFF"):
                #kolejka_zapisu.put(Rzadanie("cewka", False, self.cewka))
                Rzadanie("cewka", False, self.cewka).wykonaj();
            if (zawartosc == "ON"):
                #kolejka_zapisu.put(Rzadanie("cewka", True, self.cewka))
                Rzadanie("cewka", True, self.cewka).wykonaj();

class CzujnikBinarny:
    def __init__(self, cewka, pokoj, numer):
        self.cewka = cewka
        self.pokoj = pokoj
        self.numer = numer

        self.topic_stan = topic_prefix + "/" + pokoj + "/binarysensor/" + numer + "/state"

        drzewo[pokoj]["binarysensor"][numer] = self

    def subskrybuj(self):
        pass

    def aktualizuj(self, force):
        if ((stan_cewki_stary[self.cewka] != stan_cewki[self.cewka]) or (force)):
            if stan_cewki[self.cewka] == 0:
                wiadomosc = "OFF"
            else:
                wiadomosc = "ON"
            klient.publish(self.topic_stan, wiadomosc, retain=True);

    def wiadomosc(self, temat, zawartosc):
        pass


def on_message(client, userdata, message):
    tablica = message.topic.split("/")
    try:
        drzewo[tablica[1]][tablica[2]][tablica[3]].wiadomosc(message.topic, message.payload.decode("utf-8"))
    except Exception as error:
        #print(error)
        pass

def on_connect(client, userdata, flags, rc):
    for device in lista_obiektow:
        device.subskrybuj()
    global wymus_aktualizacje
    wymus_aktualizacje = True;
        
def przetworzDane():
    global wymus_aktualizacje
    try:
        for device in lista_obiektow:
            device.aktualizuj(wymus_aktualizacje)
        wymus_aktualizacje = False;
    except Exception as error:
        #print(error)
        wymus_aktualizacje = True;
        pass


class CustomCoilDataBlock(ModbusSequentialDataBlock):
    def setValues(self, address, value):
        #super(CustomCoilDataBlock, self).setValues(address, value)
        for x in range (0, 3900):
            stan_rejestry_stary[x] = stan_rejestry[x]
        for x in range (0, 3900):
            stan_cewki_stary[x] = stan_cewki[x]
        for x in range (8000, 8200):
            stan_rejestry_stary[x] = stan_rejestry[x]
        for index,item in enumerate(value):
            stan_cewki[address+index] = value[index]
        przetworzDane()
        #print("{} Zapis cewek".format(datetime.datetime.now()))
        #print("Zapisano {} do {}".format(value, address))

    def getValues(self, address, count=1):
        odpowiedz = super(CustomCoilDataBlock, self).getValues(address, count)
        #print("{} Odczyt cewek".format(datetime.datetime.now()))
        #print("Odczytano {} z {}".format(odpowiedz, address))
        return super(CustomCoilDataBlock, self).getValues(address, count)
    
class CustomRegisterDataBlock(ModbusSequentialDataBlock):
    def setValues(self, address, value):
        #super(CustomRegisterDataBlock, self).setValues(address, value)
        for x in range (0, 3900):
            stan_rejestry_stary[x] = stan_rejestry[x]
        for x in range (0, 3900):
            stan_cewki_stary[x] = stan_cewki[x]
        for x in range (8000, 8200):
            stan_rejestry_stary[x] = stan_rejestry[x]

        if (address >= 8000):
            for index,item in enumerate(value):
                if ( (index % 2) == 0):
                    upper = value[index+1]
                    lower = value[index]
                    combined = ((upper << 16) | lower)
                    stan_rejestry[address + (index//2) ] = combined
        else:
            for index,item in enumerate(value):
                stan_rejestry[address+index] = value[index]
        #print("{} Zapis rejestru".format(datetime.datetime.now()))
        przetworzDane()
        #print("Zapisano {} do {}".format(value, address))

    def getValues(self, address, count=1):
        odpowiedz = super(CustomRegisterDataBlock, self).getValues(address, count)
        #print("{} Odczyt rejestru".format(datetime.datetime.now()))
        #print("Odczytano {} z {}".format(odpowiedz, address))
        return super(CustomRegisterDataBlock, self).getValues(address, count)


def run_async_server():
    store = ModbusSlaveContext(
        di=CustomCoilDataBlock(0, [0]*9000),
        co=CustomCoilDataBlock(0, [0]*9000),
        hr=CustomRegisterDataBlock(0, [0]*9000),
        ir=CustomRegisterDataBlock(0, [0]*9000),
        zero_mode=True)
    context = ModbusServerContext(slaves=store, single=True)

    identity = ModbusDeviceIdentification()
    identity.VendorName = 'Pymodbus'
    identity.ProductCode = 'MM'
    identity.VendorUrl = 'http://github.com/bashwork/pymodbus/'
    identity.ProductName = 'Most MQTT'
    identity.ModelName = 'Most MQTT'
    identity.MajorMinorRevision = '1.0'
    
    StartTcpServer(context, identity=identity, address=("::", 5502))





FORMAT = ('%(asctime)-15s %(threadName)-15s'
          ' %(levelname)-8s %(module)-15s:%(lineno)-8s %(message)s')
logging.basicConfig(format=FORMAT)
log = logging.getLogger()
log.setLevel(logging.ERROR)


stan_rejestry = [0 for x in range(10000)]
stan_cewki = [0 for x in range(8000)]
stan_rejestry_stary = [0 for x in range(10000)]
stan_cewki_stary = [0 for x in range(8000)]

lista_obiektow = []
drzewo = tree()
#kolejka_zapisu = queue.Queue()
zamekMB = threading.Lock()

wymus_aktualizacje = False;

topic_prefix = "home-assistant"

client = ModbusTcpClient('192.168.10.100', 502, retries=5, retry_on_empty=True)
klient = mqtt.Client(client_id="UnitronicsHUBpi", clean_session=True, protocol=mqtt.MQTTv311, transport="tcp")

while True:
    try:
        klient.connect("127.0.0.1")
    except Exception as error:
        time.sleep(5)
        #print(error)
        pass
    else:
        break

config = json.load(open('config.json'))

for typ in config["obiekty"]:
    if typ["kategoria"] == "rolety":
        for wpis in typ["czlonkowie"]:
            roleta = Roleta(wpis["cewkaup"], wpis["cewkadown"], wpis["cewkaexec"], wpis["regstatus"], wpis["regrzad"], wpis["dlugosc"], wpis["pokoj"], wpis["id"])
            lista_obiektow.append(roleta)
    if typ["kategoria"] == "swiatla":
        for wpis in typ["czlonkowie"]:
            swiatlo = Swiatlo(wpis["cewka"], wpis["pokoj"], wpis["id"])
            lista_obiektow.append(swiatlo)
    if typ["kategoria"] == "czujniki":
        for wpis in typ["czlonkowie"]:
            czujnik = Czujnik(wpis["rejestr"], wpis["pokoj"], wpis["id"])
            lista_obiektow.append(czujnik)
    if typ["kategoria"] == "guziki":
        for wpis in typ["czlonkowie"]:
            guzik = Guzik(wpis["cewka"], wpis["cewkadl"], wpis["pokoj"], wpis["id"])
            lista_obiektow.append(guzik)
    if typ["kategoria"] == "przelaczniki":
        for wpis in typ["czlonkowie"]:
            przelacznik = Przelacznik(wpis["cewka"], wpis["pokoj"], wpis["id"])
            lista_obiektow.append(przelacznik)
    if typ["kategoria"] == "czujnikibinarne":
        for wpis in typ["czlonkowie"]:
            czujnikbinarny = CzujnikBinarny(wpis["cewka"], wpis["pokoj"], wpis["id"])
            lista_obiektow.append(czujnikbinarny)
    if typ["kategoria"] == "strefypodlewania":
        for wpis in typ["czlonkowie"]:
            strefapodlewania = StrefaPodlewania(wpis["rejestrcurr"], wpis["rejestrset"], wpis["cewkaset"], wpis["pokoj"], wpis["id"])
            lista_obiektow.append(strefapodlewania)

harmonogram = sched.scheduler(time.time, time.sleep)
def modbusKeepAlive():
    #print("keepalive")
    Rzadanie("cewka", False, 1).wykonaj();
    harmonogram.enter(60, 1, modbusKeepAlive)
harmonogram.enter(5, 1, modbusKeepAlive)

front = threading.Timer(60, harmonogram.run)
front.daemon = True
front.start()


klient.on_message=on_message
klient.on_connect=on_connect
klient.loop_start()

run_async_server()
