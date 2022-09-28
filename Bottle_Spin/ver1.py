import cv2
import math
import numpy as np
import time
from openvino.inference_engine import IECore
import subprocess
import picamera
import threading
import multiprocessing
import ctypes
import signal

draw_results = 0

#shutter_speed     = 1
#iso               = 1
#exposure_mode     = 1
#analog_gain       = 1
#digital_gain      = 1
#awb_mode          = 1
#awb_gains         = 1
capture_height    = 1280
capture_width     = 960
capture_rotation  = 90
capture_framerate = 24

#Faster R-CNN Inception v2 600x600 Aspect(H:W): 1:1; ExecTime: 1600ms
#compiled_model_path  = "./frozen_inference_graph_vpu.blob"
#threshold            = 0.8
#input_name           = "image_tensor"
#output_name          = "detection_output"
#net_height           = 600
#net_width            = 600
#stretch_aspect       = [1,1]


#SSD Mobilenet v2 300x300 Aspect(H:W): 4:3; ExecTime: 50ms
compiled_model_path  = "./mobilenet_vpu.blob"
threshold            = 0.4
input_name           = "image_tensor"
output_name          = "DetectionOutput"
net_height           = 300
net_width            = 300
stretch_aspect       = [4,3]

##
process_height = capture_height
process_width  = capture_width
desired_height = net_height * stretch_aspect[0]
desired_width  = net_width * stretch_aspect[1]

if (process_width > process_height) and (desired_height > desired_width):
	process_height, process_width = process_width, process_height

if (float(desired_width)/float(desired_height)) > (float(process_width)/float(process_height)):
	process_height = int(process_width * (float(desired_height)/float(desired_width)))
else:
	process_width = int(process_height * (float(desired_width)/float(desired_height)))


##########################################################

##### Kamera
class DoubleBuffer:
	def getLast(self):
		if self.side == 1:
			self.lock_a.acquire()
			return 1, self.lock_a
		else:
			self.lock_b.acquire()
			return 2, self.lock_b

	def isFilled(self):
		return self.filled

	def __init__(self, b1, b2, maxswaps = 0):
		self.buf_a = b1.get_obj()
		self.buf_b = b2.get_obj()
		self.lock_a = threading.Lock()
		self.lock_b = threading.Lock()
		self.lock_b.acquire()
		self.side = 1
		self.swaps = 0
		self.maxswaps = maxswaps
		self.filled = 0

	def __iter__(self):
		return self

	def __next__(self):
		if self.maxswaps > 0:
			self.swaps += 1
		if self.swaps > self.maxswaps:
			self.swaps -= 1
			raise StopIteration
		if self.side == 1:
			self.lock_a.acquire()
			self.side = 2
			self.lock_b.release()
			return self.buf_a
		else:
			self.filled = 1
			self.lock_b.acquire()
			self.side = 1
			self.lock_a.release()
			return self.buf_b


def handle_camera(output):
	camera = picamera.PiCamera()
	camera.resolution = (capture_width, capture_height)
	camera.framerate = capture_framerate
	camera.rotation = capture_rotation
	time.sleep(2) #Poczekaj na automatyczne dostrojenie jasnosci
	start = time.time()
	camera.capture_sequence(output, 'bgr', use_video_port=True)
	finish = time.time()
	print("Utworzenie klatek zajelo", finish-start)


def camera_proc(pipe, want, finished, b1, b2):
	frame_queue = DoubleBuffer(b1, b2, maxswaps=0 )
	cam_thr = threading.Thread(target = handle_camera, args=(frame_queue,), daemon=True)
	cam_thr.start()
	while frame_queue.isFilled() == 0:
		time.sleep(0.1) # Poczekaj na zebranie jakiejs klatki
	while True:
		try:
			want.wait()
			want.clear()
			state = frame_queue.getLast()
			pipe.send(state[0])
			finished.wait()
			finished.clear()
			state[1].release()
		except (EOFError, BrokenPipeError, ConnectionResetError):
			print("Kamera nie jest juz potrzebna, koncze proces")
			break

#####

##### Przygotuj wejscie

def handle_input(run_lock, cam_pip, cam_want_event, cam_copied_event, cam_buf1, cam_buf2, neur_want_event, neur_written_event, neur_buf, post_want_event, post_written_event, post_buf, thrid):
	global waiting_neural
	global waiting_post

	while True:
		run_lock.acquire()

		input_start = time.time()

		# Odbierz klatke z procesu kamery
		cam_want_event.set()
		if cam_pip.recv() == 1:
			img = np.copy(np.frombuffer(cam_buf1.get_obj(), dtype=np.uint8))
		else:
			img = np.copy(np.frombuffer(cam_buf2.get_obj(), dtype=np.uint8))
		cam_copied_event.set()


		img.shape = (capture_height, capture_width, 3)

		# Przygotuj klatke dla sieci
		rows, cols, channels = img.shape
		if (cols > rows) and (desired_height > desired_width): #Obroc wejscie aby ograniczyc straty z kadrowania
			img = cv2.rotate(img, cv2.ROTATE_90_CLOCKWISE)
			print("obracam")

		rows, cols, channels = img.shape
		if process_height != rows or process_width != cols: #Wykadruj wejscie do aspektu sieci
			img = img[0:process_height, 0:process_width].copy()

		rows, cols, channels = img.shape

		# Skurcz klatke do rozmiaru tensora
		img_conv = cv2.resize(img, (net_width, net_height)) # Ostatecznie skurcz wejscie dla tensora

		# Dostosuj format klatki
		img_conv = cv2.cvtColor(img_conv, cv2.COLOR_BGR2RGB)      #BGR -> RGB
		##img_conv = img_conv.transpose((2, 0, 1)).copy()         #HWC -> CHW
		#NCHW : C=(RGB) - Format sieci; NHWC: C=(BGR) - Format opencv


		input_end = time.time()
		print("Przetwarzanie wejsciowe klatki zajelo:  ", input_end - input_start)

		waiting_neural = thrid
		try:
			neur_want_event.wait()
			neur_want_event.clear()
			ctypes.memmove(neur_buf.get_obj(), img_conv.ctypes.data, img_conv.nbytes)
			neur_written_event.set()
		except (EOFError, BrokenPipeError, ConnectionResetError):
			break

		waiting_neural = 0

		run_lock.release()

		waiting_post = thrid
		try:
			post_want_event.wait()
			post_want_event.clear()
			ctypes.memmove(post_buf.get_obj(), img.ctypes.data, img.nbytes)
			post_written_event.set()
		except (EOFError, BrokenPipeError, ConnectionResetError):
			break

		waiting_post = 0

def ret_input_neur(neural_pipe, neural_ask, end_event):
	global waiting_neural
	while True:
		try:
			neural_ask.wait()
			neural_ask.clear()
			while waiting_neural == 0:
				if end_event.is_set():
					break
				time.sleep(0.001)
			neural_pipe.send(waiting_neural)
		except (EOFError, BrokenPipeError, ConnectionResetError):
			print("Nie czekam na neural")
			break

def ret_input_post(post_pipe, post_ask, end_event):
	global waiting_post
	while True:
		try:
			post_ask.wait()
			post_ask.clear()
			while waiting_post == 0:
				if end_event.is_set():
					break
				time.sleep(0.001)
			post_pipe.send(waiting_post)
		except (EOFError, BrokenPipeError, ConnectionResetError):
			print("Nie czekam na post")
			break

def input_proc(c_pip, c_want, c_copied, c_buf1, c_buf2, n_want1, n_written1, n_buf1, n_want2, n_written2, n_buf2, p_want1, p_written1, p_buf1, p_want2, p_written2, p_buf2, n_pip, n_ask, p_pip, p_ask, e_end):
	
	global waiting_neural
	global waiting_post
	waiting_neural = 0
	waiting_post = 0

	rlock = threading.Lock()

	hand_thr_1 = threading.Thread(target = handle_input, args=(rlock, c_pip, c_want, c_copied, c_buf1, c_buf2, n_want1, n_written1, n_buf1, p_want1, p_written1, p_buf1, 1,), daemon=True)
	hand_thr_2 = threading.Thread(target = handle_input, args=(rlock, c_pip, c_want, c_copied, c_buf1, c_buf2, n_want2, n_written2, n_buf2, p_want2, p_written2, p_buf2, 2,), daemon=True)

	ask_thr_1 = threading.Thread(target = ret_input_neur, args=(n_pip, n_ask, e_end,))
	ask_thr_2 = threading.Thread(target = ret_input_post, args=(p_pip, p_ask, e_end,))

	hand_thr_1.start()
	hand_thr_2.start()
	ask_thr_1.start()
	ask_thr_2.start()

#####

##### Uruchom siec neuronowa
def neural_proc(input_pipe, neural_ask_input, neural_want_inp1, neural_inp_copied1, neural_buf_1, neural_want_inp2, neural_inp_copied2, neural_buf_2, detected_ready, detected_copied, np_pip, detections_buf):

	ie = IECore()

	print("Ładuję model do akceleratora...", end=" ", flush=True)
	exec_net = ie.import_network(model_file=compiled_model_path, device_name="MYRIAD")
	print("Załadowano!", flush=True)

	while True:
		try:
			neural_ask_input.set()
			bufid = input_pipe.recv()
			if bufid == 1:
				neural_want_inp1.set()
				neural_inp_copied1.wait()
				neural_inp_copied1.clear()
				input_blob = np.frombuffer(neural_buf_1.get_obj(), dtype=np.uint8)
				
			else:
				neural_want_inp2.set()
				neural_inp_copied2.wait()
				neural_inp_copied2.clear()
				input_blob = np.frombuffer(neural_buf_2.get_obj(), dtype=np.uint8)

			input_blob.shape = (net_height, net_width, 3)
			input_blob = input_blob.transpose((2, 0, 1))
			input_blob.shape = (1, 3, net_height, net_width)

			neural_start = time.time()
			detections = exec_net.infer(inputs={input_name: input_blob})[output_name][0][0]
			neural_end = time.time()

			print("Siec neuronowa zajela: ", neural_end - neural_start)

			detected_ready.wait()
			detected_ready.clear()
			ctypes.memmove(detections_buf.get_obj(), detections.ctypes.data, detections.nbytes)
			detected_copied.set()
			np_pip.send(detections.size)

		except (EOFError, BrokenPipeError, ConnectionResetError):
			print("Koncze proces sieci neuronowej")
			break

#####

def waitForMyriad():
	print("Czekam na dostępny akcelerator...", end=" ", flush=True)
	good_to_go = 0
	while good_to_go == 0:
		if subprocess.run(["lsusb",  "-d",  "03e7:2485"], stdout=subprocess.DEVNULL).returncode == 0:
			good_to_go = 1 #Znaleziono NCS2 MyriadX
		if subprocess.run(["lsusb",  "-d",  "03e7:2150"], stdout=subprocess.DEVNULL).returncode == 0:
			good_to_go = 0 #Znaleziono NCS1 Myriad2 - Niekompatybilny
		if subprocess.run(["lsusb",  "-d",  "03e7:f63b"], stdout=subprocess.DEVNULL).returncode == 0:
			good_to_go = 0 #Znalezniono loopback - Trzeba poczekac na reset
		time.sleep(1)
	print("Znaleziono!", flush=True)


##### Przetwarzanie koncowe

camera_buf_1 = multiprocessing.Array('B', capture_height*capture_width*3) # Surowe klatki z kamery
camera_buf_2 = multiprocessing.Array('B', capture_height*capture_width*3) #
neural_buf_1 = multiprocessing.Array('B', net_height*net_width*3)         # Tensor dla sieci
neural_buf_2 = multiprocessing.Array('B', net_height*net_width*3)         #
post_buf_1 = multiprocessing.Array('B', process_height*process_width*3)   # Przetworzony obrazek do postprocessingu
post_buf_2 = multiprocessing.Array('B', process_height*process_width*3)   #
detections_buf = multiprocessing.Array('f', 9999*7)                       # Informacje o wykrytych obiektach

manager = multiprocessing.Manager()

pre_want_inp = manager.Event()        #Pre gotowy odebrac od kamera
pre_inp_copied = manager.Event()      #Pre skonczyl kopiowac z kamera
neural_ask_input = manager.Event()    #Neural prosi pre o id gotowego buferu
neural_want_inp1 = manager.Event()    #Neural chce zeby pre mu skopiowal
neural_inp_copied1 = manager.Event()  #Pre skonczyl kopiowac do neural
neural_want_inp2 = manager.Event()    #ditto, buf2
neural_inp_copied2 = manager.Event()  #
post_ask_input = manager.Event()      #Post prosi pre o id gotowego buferu
post_want_inp1 = manager.Event()      #Post chce zeby pre mu skopiowal
post_inp_copied1 = manager.Event()    #Pre skonczyl kopiowac do post
post_want_inp2 = manager.Event()      #ditto, buf2
post_inp_copied2 = manager.Event()    #
post_want_det = manager.Event()       #Post chce zeby neural mu skopiowal
post_det_copied = manager.Event()     #Neural skonczyl kopiowac do post
end_event = manager.Event()           #Koniec programu

ic_pip, ci_pip = multiprocessing.Pipe()
ni_pip, in_pip = multiprocessing.Pipe()
pi_pip, ip_pip = multiprocessing.Pipe()
np_pip, pn_pip = multiprocessing.Pipe()


cam_proc = multiprocessing.Process(target=camera_proc, args=(ci_pip, pre_want_inp, pre_inp_copied, camera_buf_1, camera_buf_2,))

in_proc = multiprocessing.Process(target=input_proc, args=(ic_pip, pre_want_inp, pre_inp_copied, camera_buf_1, camera_buf_2, neural_want_inp1, neural_inp_copied1, neural_buf_1, neural_want_inp2, neural_inp_copied2, neural_buf_2, post_want_inp1, post_inp_copied1, post_buf_1, post_want_inp2, post_inp_copied2, post_buf_2, in_pip, neural_ask_input, ip_pip, post_ask_input, end_event,))

neu_proc = multiprocessing.Process(target=neural_proc, args=(ni_pip, neural_ask_input, neural_want_inp1, neural_inp_copied1, neural_buf_1, neural_want_inp2, neural_inp_copied2, neural_buf_2, post_want_det, post_det_copied, np_pip, detections_buf,))


waitForMyriad()

original_sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)

cam_proc.start()
in_proc.start()
neu_proc.start()

signal.signal(signal.SIGINT, original_sigint_handler)

#

while True:
	try:
		lastframe = time.time()

		post_ask_input.set()
		bufid = pi_pip.recv()

		if bufid == 1:
			post_want_inp1.set()
			post_inp_copied1.wait()
			post_inp_copied1.clear()
			img = np.frombuffer(post_buf_1.get_obj(), dtype=np.uint8)
			img.shape = (process_height, process_width, 3)
		else:
			post_want_inp2.set()
			post_inp_copied2.wait()
			post_inp_copied2.clear()
			img = np.frombuffer(post_buf_2.get_obj(), dtype=np.uint8)
			img.shape = (process_height, process_width, 3)

		rows, cols, channels = img.shape

		post_want_det.set()
		post_det_copied.wait()
		post_det_copied.clear()

		det_val_num = pn_pip.recv()
		detections = np.frombuffer(detections_buf.get_obj(), dtype=np.float32, count=det_val_num)
		detections.shape = (int(det_val_num/7),7)
		
		start = time.time()

		for detection in detections:
			klasa = int(detection[1])
			color = (0,0,0)
			nazwa = "NIEZNANE"
			if klasa == 1:
				color = (0,0,255)
				nazwa = "butelka"
			if klasa == 2:
				color = (0,255,0)
				nazwa = "nakretka"
			score = float(detection[2])
			if score > threshold:
				print("Znaleziono obiekt o klasie:", klasa, "-", nazwa)
				print("Pewnosc: ", score)
				left = detection[3] * cols
				top = detection[4] * rows
				right = detection[5] * cols
				bottom = detection[6] * rows

				if klasa == 1:
					but_start = time.time()

					bb_height = bottom - top
					bb_width  = right - left
					padding = int(min(bb_height, bb_width) * 0.05)
					roi = img[ max(int(top)-padding-2,0)  :  min(int(bottom)+padding+2,rows)  ,  max(int(left)-padding-2,0)  :  min(int(right)+padding+2,cols) ]

					monochrom = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)
					ret,monochrom = cv2.threshold(monochrom, 0, 255, cv2.THRESH_OTSU)
					#kernel = np.ones((5,5), np.uint8)
					#monochrom = cv2.morphologyEx(monochrom, cv2.MORPH_CLOSE, kernel) #zalataj luki z otsu

					contours, hierarchy = cv2.findContours(monochrom, cv2.RETR_LIST, cv2.CHAIN_APPROX_SIMPLE)
					if len(contours) == 0:
						print("Blad przetwarzania butelki")
						continue
					maxcont = max(contours, key=lambda x: cv2.contourArea(x))
					cntsort = []
					cntsort.append(maxcont)

					if draw_results == 1:
						cv2.drawContours(roi, cntsort, 0, (0,255,0), 10) ## podglad konturu butelki

					rotrect = cv2.minAreaRect(maxcont)
					box = cv2.boxPoints(rotrect)

					dist1 = math.sqrt( pow((box[0][0] - box[1][0]),2) + pow((box[0][1] - box[1][1]),2) )
					dist2 = math.sqrt( pow((box[1][0] - box[2][0]),2) + pow((box[1][1] - box[2][1]),2) )
					#print("Dlugosc pierwszego boku prostokatu ograniczajacego butelke :",dist1)
					#print("Dlugosc drugiego boku prostokatu ograniczajacego butelke :",dist2)

					box = np.int0(box)

					angle = -rotrect[2]
					if dist1 < dist2:
						angle = angle + 90


					(h, w) = monochrom.shape[:2]
					(cX, cY) = (w // 2, h // 2)
					M = cv2.getRotationMatrix2D((cX, cY), -angle, 1.0)
					cos = np.abs(M[0, 0])
					sin = np.abs(M[0, 1])
					nW = int((h * sin) + (w * cos))
					nH = int((h * cos) + (w * sin))
					M[0, 2] += (nW / 2) - cX
					M[1, 2] += (nH / 2) - cY


					transbox = cv2.transform(np.array([box]), M)
					top2    = max(int(min(transbox[0][0][1], transbox[0][1][1], transbox[0][2][1], transbox[0][3][1]))+1 , 1)
					bottom2 = min(int(max(transbox[0][0][1], transbox[0][1][1], transbox[0][2][1], transbox[0][3][1]))+1, nH+1)
					left2   = max(int(min(transbox[0][0][0], transbox[0][1][0], transbox[0][2][0], transbox[0][3][0]))+1, 1)
					right2  = min(int(max(transbox[0][0][0], transbox[0][1][0], transbox[0][2][0], transbox[0][3][0]))+1, nW+1)

					transformed_cont = cv2.transform(maxcont, M)
					cntsort_tr = []
					cntsort_tr.append(transformed_cont)

					transformed_mono = np.zeros((nH+2, nW+2, 1), dtype=np.uint8)

					hull_list = []
					hull_list.append(cv2.convexHull(transformed_cont))


					cv2.drawContours(transformed_mono, hull_list, 0, 255, -1, offset=(1,1))
					cv2.drawContours(transformed_mono, cntsort_tr,  0, 0  , -1, offset=(1,1))

					midheight = int(top2 + ((bottom2 - top2)/2))
					concave_up   = cv2.countNonZero(transformed_mono[ top2 : midheight  ,  left2 : right2 ])
					concave_down = cv2.countNonZero(transformed_mono[ midheight : bottom2  ,  left2 : right2 ])
					print("Ilosc wkleslych pikseli w gornej polowie", concave_up)
					print("Ilosc wkleslych pikseli w dolnej polowie", concave_down)

					kat = angle + 90
					if (concave_up < concave_down):
						kat = kat + 180

					kat_rad = (kat/180)*math.pi
					print("Kat nachylenia butelki to:", kat%360)

					if draw_results == 1:
						cv2.arrowedLine(img2, ( int( (img2.shape[1])/2 ) , int( (img2.shape[0])/2 )), ( int(((img2.shape[1])/2)+(300*math.cos(kat_rad))) , int(((img2.shape[0])/2)-(300*math.sin(kat_rad)))  ), (255,0,0), 20 )
					but_end = time.time()
					print("Przetwarzanie butelki :", but_end - but_start)

				#Zaznacz wykryty obiekt prostokatem
				if draw_results == 1:
					text = nazwa + " : " + "{:.0f}".format(score*100.0) + "%"
					cv2.rectangle(img, (int(left), int(top)), (int(right), int(bottom)), color, thickness=10)
					rozmiartextu = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 3, 6)
					cv2.rectangle(img, (int(left), int(top)), (int(left)+rozmiartextu[0][0]+10, int(top)-rozmiartextu[0][1]-10), color, thickness=cv2.FILLED)
					cv2.putText(img, text, (int(left), int(top)), cv2.FONT_HERSHEY_SIMPLEX, 3, (0,0,0), 6)
				print("")
		done = time.time()
		print("Obrobka koncowa zajela: " + str(done-start))
		thisframe = time.time()
		print("FPS: ", 1/(thisframe-lastframe))
	except KeyboardInterrupt:
		break
try:
	end_event.set()
except:
	pass
