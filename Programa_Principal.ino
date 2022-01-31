// Bibliotecas //
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pinos de leitura (Entrada) //
#define pinAjusteTemp 14  // A0
#define pinTermistor1 17  // A3
#define pinTermistor2 16  // A2
#define pinTermistor3 20  // A6
#define pinInterruptDisplay 2
#define pinInterruptCooler  3

// Pinos de escrita (Saída) //
#define pinAlimentarDisplay 4
#define pinAlimentarCooler 5

// Parâmetros do circuito do termistor //
#define Vcc 5
#define R0_1 9880.0
#define R0_2 9865.0
#define R0_3 9870.0
#define Rx 0.0566878 // Rx = R0_t * exp(-3600.0/298.0), R0_t = 10000
#define amostras 20
#define tam_vetor 300

// Variáveis principais criadas //
unsigned long int tempo_ligado    = -1;
unsigned long int tempo_controle1 = 0;
unsigned long int tempo_controle2 = 0;
bool  estado_cooler_manual        = 0;
bool  estado_cooler_auto          = 0;
bool  estado_display              = 1;
bool  reiniciar_display           = 0;
double soma                       = 0;
byte  t_ajuste                    = 0;
float t_gelo                      = 0.0;
float t_med                       = 0.0;
float t_est[tam_vetor];

// Matriz de caracteres para o LCD //
byte grau[8] = {
  0b11000,
  0b11000,
  0b00111,
  0b01000,
  0b01000,
  0b01000,
  0b00111,
  0b00000
};
byte limpar[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};
byte sim[8] = {
  0b01110,
  0b10001,
  0b10000,
  0b01110,
  0b00001,
  0b10001,
  0b01110,
  0b00000
};

// Delaração de funções //
void mostrar_conteudo();
void calcular_temperatura();
void mudar_estado_display();
void mudar_estado_cooler();

// Objeto LCD //
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {

  // Setar referência analógica como a saída do regulador de tensão //
  analogReference(EXTERNAL);

  // Modos dos pinos //
  pinMode(pinAlimentarCooler, OUTPUT);
  pinMode(pinInterruptCooler, INPUT);
  pinMode(pinAlimentarDisplay, OUTPUT);
  pinMode(pinInterruptDisplay, INPUT);

  // Interrupção para ligar e desligar LCD e Cooler manualmente//
  attachInterrupt(digitalPinToInterrupt(pinInterruptDisplay), mudar_estado_display, RISING);
  attachInterrupt(digitalPinToInterrupt(pinInterruptCooler), mudar_estado_cooler, RISING);

  // Preencher t_est com valores iniciais que não aproximem a inclinação de 0 //
  for (int i = 0; i < tam_vetor; i++) {
    t_est[i] = 100 + 10 * i;
  }

  // Inicia o lcd, ativa luz, cursor sem piscar //
  digitalWrite(pinAlimentarDisplay, HIGH);
  lcd.init();
  lcd.backlight();
  lcd.noCursor();

  // Cria os caracteres para o LCD //
  lcd.createChar(0, limpar);
  lcd.createChar(1, grau);
  lcd.createChar(2, sim);
}

void loop() {
  soma = 0;
  
  // Loop Manual //
  vaipara_loop_manual:
  if(estado_cooler_manual == 1){
    if(millis() - tempo_controle1 >= 750){

      t_ajuste = map(analogRead(pinAjusteTemp), 0, 1024, 1, 31);

      calcular_temperatura();

      if(estado_display) mostrar_conteudo();
      
      tempo_controle1 = millis();

    }
    goto vaipara_loop_manual;
  }

  // Loop automático //
  if (millis() - tempo_controle2 >= 600) {

    t_ajuste = map(analogRead(pinAjusteTemp), 0, 1024, 1, 31);

    calcular_temperatura();

    if (t_med >= t_ajuste) {
      digitalWrite(pinAlimentarCooler, HIGH);
      estado_cooler_auto = 1;
      tempo_ligado = millis();
    }

    if ((millis() - tempo_ligado >= 10000) && (tempo_ligado != -1)) {

      soma += abs(((t_est[60] - t_est[0]) / 60));
      soma += abs(((t_est[120] - t_est[60]) / 60));
      soma += abs(((t_est[180] - t_est[120]) / 60));
      soma += abs(((t_est[240] - t_est[180]) / 60));
      soma += abs(((t_est[299] - t_est[240]) / 59));
      soma += abs(((t_est[299] - t_est[0]) / 299));

      // 0.052408 ~= 3° 0.034921 ~= 2° , 0.017455 ~= 1°, 0.008727 ~= 0.5°, 0.004363 ~= 0.25°//
      if (soma/6 <= 0.004363) {
        digitalWrite(pinAlimentarCooler, LOW);
        estado_cooler_auto = 0;
        tempo_ligado = -1;
      }
    }
  
    if(estado_display) mostrar_conteudo();

    tempo_controle2 = millis();
  }
}

void calcular_temperatura() {
  // Ler o sensor algumas vezes //
  unsigned long int soma1 = 0, soma2 = 0, soma3 = 0, i = 0;
  unsigned long ultimo = 0;
  while (1) {
    if (millis() - ultimo >= 10) {
      ultimo = millis();
      if (i < amostras) {
        soma1 += analogRead(pinTermistor1);
        soma2 += analogRead(pinTermistor2);
        soma3 += analogRead(pinTermistor3);
        ++i;
      } else break;
    }
  }

  // Determina a resistência do termistor //
  double V1 = (Vcc * soma1) / (amostras * 1024.0);
  double V2 = (Vcc * soma2) / (amostras * 1024.0);
  double V3 = (Vcc * soma3) / (amostras * 1024.0);
  double Rt1 = (Vcc * R0_1) / V1 - R0_1;
  double Rt2 = (Vcc * R0_2) / V2 - R0_2;
  double Rt3 = (Vcc * R0_3) / V3 - R0_3;

  // Calcula a temperatura //
  t_med = (3600.0 / log(Rt1 / Rx) + 3600.0 / log(Rt2 / Rx)) / 2 - 273;
  t_gelo = 3600.0 / log(Rt3 / Rx) - 273;

  // Descola o vetor para a direita, ignorando t_est[0] //
  for (int i = tam_vetor - 1; i > 0; i--) {
    t_est[i] = t_est[i - 1];
  }

  // Adicina a próxima leitura no começo do vetor //
  t_est[0] = t_med;
}

void mostrar_conteudo() {
  
  if(reiniciar_display == 1){
    lcd.init();
    lcd.backlight();
    lcd.noCursor();
    lcd.createChar(0, limpar);
    lcd.createChar(1, grau);
    lcd.createChar(2, sim);
    lcd.clear();
    reiniciar_display = 0;
  }

  lcd.home();

  if (t_med < 10 and t_med >= 0) {
    lcd.print("TM:0");
    lcd.print((int) t_med);
    lcd.write(1);
  } else if (t_med < 0 and t_med > -10) {
    lcd.print("TM:");
    lcd.print((int) t_med);
    lcd.write(1);
    lcd.setCursor(6, 0);
    lcd.write(0);
  } else {
    lcd.print("TM:");
    lcd.print((int) t_med);
    lcd.write(1);
  }

  lcd.setCursor(0, 1);

  if (t_gelo < 10 and t_gelo >= 0) {
    lcd.print("TG:0");
    lcd.print((int) t_gelo);
    lcd.write(1);
  } else if (t_gelo < 0 and t_gelo > -10) {
    lcd.print("TG:");
    lcd.print((int) t_gelo);
    lcd.write(1);
    lcd.setCursor(6, 1);
    lcd.write(0);
  } else {
    lcd.print("TG:");
    lcd.print((int) t_gelo);
    lcd.write(1);
  }

  lcd.setCursor(8, 0);

  lcd.print("TA:");
  lcd.print(t_ajuste);
  lcd.write(1);

  lcd.setCursor(8, 1);

  lcd.print("Auto:");
  if(estado_cooler_manual == 0){
    lcd.write(2);
  }else{
    lcd.print("N");
  }

  lcd.home();
}

void mudar_estado_display() {

  if (estado_display == 0) {
    digitalWrite(pinAlimentarDisplay, HIGH);
    estado_display = 1;
    reiniciar_display = 1;
    return;
  }

  if (estado_display == 1) {
    digitalWrite(pinAlimentarDisplay, LOW);
    estado_display = 0;
    return;
  }

}

void mudar_estado_cooler(){
  
  if(estado_cooler_manual == 0){
    estado_cooler_manual = 1;
  }else{
    estado_cooler_manual = 0; 
  }
  if(estado_cooler_auto == 0){
    digitalWrite(pinAlimentarCooler, estado_cooler_manual);
  }
}
