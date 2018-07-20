#include <stdlib.h>		// biblioteca para usar exit()
#include <stdio.h>		// biblioteca para usar fprintf()
#include <string.h>		// biblioteca para usar strerror()
#include <pthread.h>	// biblioteca para usar threads
#include <unistd.h>		// biblioteca para acessar a API POSIX
#include <bcm2835.h>	// biblioteca para controle do GPIO
#include <time.h>       // biblioteca que servir� para dar nome as imagens 

//bibliotecas para processar imagens
#include <opencv2/opencv.hpp> 
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core.hpp>

#define HANDSQUANTITY 2
#define NUMBERFINGERS 5
#define AMOUNTCAPTUREFINGER 5

using namespace std;
using namespace cv;

struct button {
	uint8_t btn;
	int btn_state;
};
typedef struct button BUTTON;
struct seaid_project {
	/*/
	uint8_t ledGreen = RPI_V2_GPIO_P1_37;
	uint8_t ledRed = RPI_V2_GPIO_P1_31;
	uint8_t ledYellow = RPI_V2_GPIO_P1_35;
	uint8_t ledReader = RPI_V2_GPIO_P1_33;
	*/
	uint8_t ledGreen ;
	uint8_t ledRed ;
	uint8_t ledYellow;
	uint8_t ledReader ;
	BUTTON btnShutdown;
	BUTTON btnReboot;;
};
typedef struct seaid_project SEAID;


/* Prot�tio de Fun��es */
void *controlSystem(void *p);
void *inputRead(void *p);
void *captureImage(void *p);
void startCapture(SEAID seaid);
void error(uint8_t led, String message);
bool hasFingerprint(Mat frame);
pthread_mutex_t lockMutex;	//mutex

/***************************************************************
 * Fun��o principal (thread principal)
 **************************************************************/
int main(int argc, char **argv) {

	SEAID seaid;
	seaid.btnShutdown.btn = RPI_V2_GPIO_P1_16;
	seaid.btnReboot.btn = RPI_V2_GPIO_P1_18;
	seaid.ledGreen = RPI_V2_GPIO_P1_37;
	seaid.ledRed = RPI_V2_GPIO_P1_31;
	seaid.ledYellow = RPI_V2_GPIO_P1_35;
	seaid.ledReader = RPI_V2_GPIO_P1_33;
	pthread_t t_btnShutdown, t_btnReboot, t_captureImage;
	int ret;
	


	// Inicializa mutex
	if (pthread_mutex_init(&lockMutex, NULL) != 0) {
		error(seaid.ledRed, "Erro ao inicializar mutex.\n");
	}
	// Iniciliza biblioteca bcm2835
	if (!bcm2835_init()) {
		error(seaid.ledRed, "Erro ao inicializar biblioteca bcm2835.\n");
	}
	bcm2835_gpio_write(seaid.ledGreen, HIGH);
	bcm2835_gpio_write(seaid.ledYellow, LOW);
	bcm2835_gpio_write(seaid.ledRed, LOW);
	bcm2835_gpio_write(seaid.ledReader, LOW);
	// Seleciona pinos de sa�da para LEDs
	bcm2835_gpio_fsel(seaid.ledGreen, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(seaid.ledRed, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(seaid.ledYellow, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(seaid.ledReader, BCM2835_GPIO_FSEL_OUTP);

	// Seleciona pinos de entrada para push buttons
	// Desabilita resistores de pull up/down nesses pinos
	bcm2835_gpio_fsel(seaid.btnShutdown.btn, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(seaid.btnShutdown.btn, BCM2835_GPIO_PUD_OFF);
	bcm2835_gpio_fsel(seaid.btnReboot.btn, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(seaid.btnReboot.btn, BCM2835_GPIO_PUD_OFF);


	//Cria thread para OpenCV
	ret = pthread_create(&t_captureImage, NULL, captureImage, &seaid);
	if (ret != 0) {
		error(seaid.ledRed, "Erro thread \"t_captureImage\"");
	}

	// Cria thread para ler estado dos botões
	ret = pthread_create(&t_btnShutdown, NULL, inputRead, &seaid.btnShutdown);
	if (ret != 0) {
		error(seaid.ledRed, "Erro thread \"t_btnShutdown\"");
	}
	ret = pthread_create(&t_btnReboot, NULL, inputRead, &seaid.btnReboot);
	if (ret != 0) {
		error(seaid.ledRed, "Erro thread \"t_btnReboot\"");
	}
	// Aguarda as threads terminarem
	pthread_join(t_captureImage, NULL);
	pthread_join(t_btnShutdown, NULL);
	pthread_join(t_btnReboot, NULL);

	// Finaliza biblioteca bcm2835
	bcm2835_close();
	//Destr�i o mutex
	pthread_mutex_destroy(&lockMutex);

	return 0;
}
/***************************************************************
* Funa��o respons�vel por exibir as mensagens de erro
* e fechar a aplica��o
**************************************************************/
void error(uint8_t led, String message) {
	bcm2835_gpio_write(led, HIGH);
	fprintf(stderr, message.c_str());
	exit(EXIT_FAILURE);
}
/***************************************************************
* Funa��o respons�vel por controlar os LEDs
**************************************************************/
void *controlSystem(void *p) {
	// Faz o cast do ponteiro para voltar a ser um inteiro
	SEAID *seaid = (SEAID *)p;

	//Loop infinito 
	while (1) {
		// Trava o mutex antes de acessar variável led_state
		pthread_mutex_lock(&lockMutex);
		// Atualiza estado do LED - liga ou desliga
		if (seaid->btnShutdown.btn_state)
			system("sudo poweroff");

		if (seaid->btnReboot.btn_state)
			system("sudo reboot");
		
		// Libera o mutex
		pthread_mutex_unlock(&lockMutex);
		// Pequeno delay
		bcm2835_delay(50);
	}
	pthread_exit(NULL);
}

/***************************************************************
* Fun��o respons�vel pela leitura dos bot�es
**************************************************************/
void *inputRead(void *p) {
	// Faz o cast do ponteiro para voltar a ser um inteiro
	BUTTON *button = (BUTTON *)p;

	//Loop infinito 
	while (1) {
		// Trava o mutex antes de acessar variável led_state
		pthread_mutex_lock(&lockMutex);
		// Atualiza estado do botão na variável
		button->btn_state = bcm2835_gpio_lev(button->btn);
		// Libera o mutex
		pthread_mutex_unlock(&lockMutex);
		// Pequeno delay
		bcm2835_delay(50);
	}
	pthread_exit(NULL);
}
/***************************************************************
* Fun��o respons�vel por verificar se h� algum volunt�rio na
* m�quina para a raliza��o da captura da digital. Caso positivo,
* ser� iniciada a captura das digitais.
**************************************************************/
void *captureImage(void *p) {
	
	SEAID *seaid = (SEAID *)p;
	std::vector<Rect> digitais;
	bcm2835_gpio_write(seaid->ledReader, HIGH);
	VideoCapture cap(0);
	Mat frame;

	cap >> frame;
	if (frame.empty()) {
		error(seaid->ledRed, "Erro ao capturar a imagem");
	}
	try {
		if (hasFingerprint(frame)) {
			try {
				startCapture(*seaid);
			}
			catch (...) {
				error(seaid->ledRed, "Erro captura das digitais do volunt�rio");
			}
			pthread_mutex_unlock(&lockMutex);
			bcm2835_gpio_write(seaid->ledReader, LOW);

		}
		else {
			pthread_mutex_unlock(&lockMutex);
			bcm2835_gpio_write(seaid->ledReader, LOW);

		}
	}catch(...) {
		error(seaid->ledRed, "Erro na detec��o da imagem");
	}
	pthread_exit(NULL);
}

/***************************************************************
* Fun��o respons�vel por veririficar se a imagem que est� 
* sendo exibida � uma impress�o digital ou n�o.
**************************************************************/
bool hasFingerprint(Mat frame)
{
	bool hasImage = false;
	std::vector<Rect> fingerprints;

	CascadeClassifier fingerPrintCascade;
	fingerPrintCascade.load("Training/data/cascade.xml");

	try {
		fingerPrintCascade.detectMultiScale(frame, fingerprints, 1.1, 0, 0 | CASCADE_SCALE_IMAGE, Size(30, 30));
	}
	catch (...) {
		
	}
	if (!fingerprints.empty()) {
		hasImage = true;
	}
	return hasImage;
}

/***************************************************************
* Fun��o respons�vel pela captura de imagens para a
* constru��o de um banco de digitais de volunt�rios
**************************************************************/
void startCapture(SEAID seaid) {
	VideoCapture cap(0);
	Mat frame;
	cap >> frame;
	system("mkdir $HOME/Fingerprints 2> /dev/null");
	
	bcm2835_gpio_write(seaid.ledGreen, LOW);
	bcm2835_gpio_write(seaid.ledYellow, HIGH);
	for (int i = 0; i < (HANDSQUANTITY*NUMBERFINGERS*AMOUNTCAPTUREFINGER); i++) {
		try {
			while (!hasFingerprint(frame)) {
				cap >> frame;
			}
			string nome = to_string(time(NULL)) + ".jpeg";
			imwrite(nome, frame);
			delay(1000);
		}
		catch (...) {
			error(seaid.ledRed, "Erro ao capturar imagem da impress�o digital do volunt�rio.");
		}
	}
	bcm2835_gpio_write(seaid.ledGreen, HIGH);
	bcm2835_gpio_write(seaid.ledYellow, LOW);

}
