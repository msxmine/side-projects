import logging
import sys
from time import time

from openvino.inference_engine import IENetwork, IECore, IEPlugin

logging.basicConfig(format="[ %(levelname)s ] %(message)s", level=logging.INFO, stream=sys.stdout)
log = logging.getLogger()

model_xml = "./mobilenet.xml"
model_bin = "./mobilenet.bin"
model_exp = "./mobilenet_vpu.blob"

log.info("Loading network files:\n\t{}\n\t{}".format(model_xml, model_bin))
net = IENetwork(model=model_xml , weights=model_bin)
net.inputs["image_tensor"].precision = 'FP32'
net.outputs["detection_output"].precision = 'FP32'
#plugin = IEPlugin(device="MYRIAD")
ie = IECore()
#exec_net = plugin.load(network=net, num_requests=2)
exec_net = ie.load_network(network=net, device_name="MYRIAD")
exec_net.export(model_exp)
log.info("Exporting executable network:\n\t{}".format(model_exp))

