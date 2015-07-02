#include <IRremote.h>
#include <bassdll.h>
#include <LiquidCrystal.h>

//LEITURAS DO CONTROLE REMOTO (results.value)
#define CIMA        551486205
#define BAIXO       551518845
#define ESQUERDA    551542815
#define DIREITA     551510175
#define SELECT      551494365

/*  PARA IMPLEMENTAR
 *    usar infravermelho (mega)
 *    implementar campainha
 *    tentar implementar sensor de humidade
 *    party mode
 */

/*

TIPOS DE PINO
B (d8 ate d13)
C (a0  ate a5)
D (d0  ate d7)

ALTERANDO REGISTRADORES
     posicao  <-  43210
DDRD =        B11111110;  // configura de d1 ate d7 como saida, d0 como entrada
DDRD = DDRD | B11111100;  // ou bit-a-bit para alterar 1s para 0s
PORTD =       B10101000;  // d7-d5-d3 para HIGH

CONFIGURANDO INTERRUPCOES
attachInterrupt(0,               interrupcao,       RISING;
                0-pd2, 1-pd3     funcao             LOW, CHANGE, RISING, FALLING, HIGH
            
*/


//VARIAVEIS GLOBAIS [COMPARTILHADAS E QUE NAO PODEM SER DESTRUIDAS NO FIM DO LOOP()]
int anterior      = 0; // para utilizar em millis()
int executado     = 0; // para controlar se codigo ja foi executado ao menos uma vez
int configLuz     = 4; // inicializa luz de fundo em automatico (-1:off,0:min,1:med,2:max,3:mmax,4:auto)
int configFundo   = 4; // inicializa com iluminacao autom.      (0:min,1:med,2:max,3:mmax,4:auto)
int configModo    = 0; // modo de operacao da tela              (0-informacoes,1-leituraBrutaLdr,2-calibracao)
int botoeira      = 0; // status da botoeira

int pwmLed1       = 0; // declaradas como globais, pois iluminacao precisa ser preservada em todas as iteracoes
int pwmLed2       = 0;

int atual         = 0; // utilizada para armazenar tempo atual nos procedimentos envolvendo millis()
int anteriorLed   = 0; // ...

int niveisLuz[4] = {40, 100, 170, 255};


//LUMINOSIDADE IDEAL MEDIA SOBRE LDR: 620
int luminosidadeIdeal  = 620; // revisar!

//PINOS
//const int INFRAVERMELHO = 2;
const int BOTAO         = 2;
const int LED1          = 3;
const int BACKLIGHT     = 10;
const int LED2          = 11;

//INICIALIZACAO DO MIXER DE 3 CANAIS
#define EIGHTH    18
#define KICK_LEN  9
#define SNARE_LEN 7
mixer m;
note** blocoDeMemoria;
channel pin12  (12,  1);
channel pin13 (13, 1);
channel pinA5 (A5, 1);

//INICIALIZACAO DA BIBLIOTECA DE LCD E RELACIONADOS
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
#define btnDIR     0
#define btnCIMA    1
#define btnBAIXO   2
#define btnESQ     3
#define btnSELECT  4
#define btnNENHUM  5
int leituraBotao = 0; // recebera leitura analogica
int valorLido    = 0; // recebera leitura processada

//INICIALIZACAO DO MODULO DE INFRAVERMELHO
//IRrecv irrecv(INFRAVERMELHO);
decode_results results;

//CARACTERES ESPECIAIS CRIADOS PARA IMPRIMIR NA TELA
byte char1[8] = {
        B00000,
        B00001,
        B00010,
        B10100,
        B01000,
        B00000,
        B00000,
        B00000
};

byte char2[8] = {
        B11000,
        B11000,
        B11000,
        B11000,
        B11000,
        B11000,
        B11111,
        B01111
};

byte char3[8] = {
        B00011,
        B00011,
        B00011,
        B00011,
        B00011,
        B00011,
        B11111,
        B11110
};

byte char4[8] = {
        B00000,
        B10000,
        B01000,
        B00101,
        B00010,
        B00000,
        B00000,
        B00000
};

byte contrabarra[8] = {
        B00000,
        B10000,
        B01000,
        B00100,
        B00010,
        B00001,
        B00000,
        B00000
};

byte nivel1[8] = {
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00000,
        B00011,
        B11111
};

byte nivel2[8] = {
        B00000,
        B00000,
        B00000,
        B00000,
        B00011,
        B11111,
        B11111,
        B11111
};

byte nivel3[8] = {
        B00000,
        B00000,
        B00011,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111
};

byte nivel4[8] = {
        B00011,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111,
        B11111
};

void LeituraLdr();
void LuminosidadeIdeal();
void Abertura();
int LerBotao();
void FadeLuz(int pino, int tipo);
void AlteraGrafico(int entrada);
void ImprimeSegundaLinha();
void ProcessaEntrada();
void Melodia(channel &segundaVoz, channel &baixo, channel &melodia);
void InfravermelhoSerial();

void setup() {
  //irrecv.enableIRIn();
  //Serial.begin(9600);
  
  pinMode(BOTAO, INPUT_PULLUP);
  analogWrite(BACKLIGHT, 0);

  lcd.begin(16, 2);              // inicia a biblioteca
  
  Abertura();
  
  for(int i = 0; i < 8; i++) {   // reseta caracteres usados na logo
    char1[i] = 0;
    char2[i] = 0;
    char3[i] = 0;
    char4[i] = 0;
  }
  
  lcd.createChar(0, char1);
  lcd.createChar(1, char2);
  lcd.createChar(2, char3);
  lcd.createChar(3, char4);
  
  lcd.createChar(4, nivel1);     // insere caracteres de nivel nos buffers finais
  lcd.createChar(5, nivel2);
  lcd.createChar(6, nivel3);
  lcd.createChar(7, nivel4);
}

void loop() {
  if(executado == 0)         // limpar a tela na primeira execucao
    lcd.clear();
  
  //if(irrecv.decode(&results))
  //  irrecv.resume(); // recebe entrada do infravermelho
  //InfravermelhoSerial();

  ProcessaEntrada();

  int mediaLeituraLdrs = (analogRead(A1) + analogRead(A2) + analogRead(A3) + analogRead(A4)) / 4; // medias de leituras dos LDRs
  int entradaGrafico = round((float)mediaLeituraLdrs / 127.875);                           // de 0 a 8, para display
  
  atual = millis();
  if(((atual - anterior) >= 300) && executado == 1) { // transcorridos 300ms, atualizar grafico
    anterior = millis();
    AlteraGrafico(entradaGrafico);      // altera caracteres do grafico
  }
  
  if(configModo == 0) {
    lcd.setCursor(0, 0);                  // imprime primeira linha
      lcd.print("LCD   Luz  Graf.");
    ImprimeSegundaLinha();
  } else if(configModo == 1) {
      LeituraLdr();
  } else if(configModo == 2) {
      lcd.setCursor(0, 0);
      lcd.print("Lumin. Ideal:   ");
      lcd.setCursor(0, 1);
      lcd.print(luminosidadeIdeal);
      lcd.print("                ");   
  }
  
  if(executado == 0)                    // atualiza flag da primeira execucao em diante e acende tela
    FadeLuz(BACKLIGHT, 1);
  
  if(configFundo == 4) {                                    // ajuste de backlight
    if(mediaLeituraLdrs/4 > 10)                             // leitura maxima: 1023, divide por 4 para aproximar de 255
      analogWrite(BACKLIGHT, mediaLeituraLdrs/4);           /* escreve no LCD algo entre 0-255, para nao cansar a visao
                                                             * no escuro e oferecer alto brilho em ambientes claros */
                                                             
    else analogWrite(BACKLIGHT, 15);                        // 15 eh a iluminacao minima
  } else analogWrite(BACKLIGHT, niveisLuz[configFundo]);

  if(configLuz >= 0 && configLuz < 4) {         // ajustes-padrao
    analogWrite(LED1, niveisLuz[configLuz]);
    analogWrite(LED2, niveisLuz[configLuz]);
  } else if(configLuz == 4)                     // ajuste de luminosidadeIdeal
    LuminosidadeIdeal(); 
  else if(configLuz == -1) {
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);  
  }
  
  executado = 1;
}

void LeituraLdr() {
  int s1 = analogRead(A1), s2 = analogRead(A2),
      s3 = analogRead(A3), s4 = analogRead(A4);
  
  lcd.setCursor(0, 0);
  lcd.print("s1  s2  s3  s4  ");

  lcd.setCursor(0, 1);            // imprime ilum. LCD
  if(s1 < 10) {
    lcd.print(s1);
    lcd.print("   ");
  } else if(s1 < 100) {
    lcd.print(s1);
    lcd.print("  ");
  } else if(s1 < 1000) {
    lcd.print(s1);
    lcd.print(" ");
  } else
    lcd.print(s1);
    
  lcd.setCursor(4, 1);            // imprime ilum. LCD
  if(s2 < 10) {
    lcd.print(s2);
    lcd.print("   ");
  } else if(s2 < 100) {
    lcd.print(s2);
    lcd.print("  ");
  } else if(s2 < 1000) {
    lcd.print(s2);
    lcd.print(" ");
  } else
    lcd.print(s2);
    
  lcd.setCursor(8, 1);            // imprime ilum. LCD
  if(s3 < 10) {
    lcd.print(s3);
    lcd.print("   ");
  } else if(s3 < 100) {
    lcd.print(s3);
    lcd.print("  ");
  } else if(s3 < 1000) {
    lcd.print(s3);
    lcd.print(" ");
  } else
    lcd.print(s3);
    
  lcd.setCursor(12, 1);            // imprime ilum. LCD
  if(s4 < 10) {
    lcd.print(s4);
    lcd.print("   ");
  } else if(s4 < 100) {
    lcd.print(s4);
    lcd.print("  ");
  } else if(s4 < 1000) {
    lcd.print(s4);
    lcd.print(" ");
  } else
    lcd.print(s4);
  
}

void LuminosidadeIdeal() {
  int regiao1 = (analogRead(A1) + analogRead(A2)) / 2;      // media da leitura na regiao 1
  int regiao2 = (analogRead(A3) + analogRead(A4)) / 2;      // media da leitura na regiao 2
  atual = anteriorLed = millis();
  while((atual - anteriorLed) < 150) { // oferece transicoes suaves
    if(regiao1 > luminosidadeIdeal) {
      if(pwmLed1 > 0)  
        analogWrite(LED1, --pwmLed1);
    } if(regiao1 < luminosidadeIdeal) {
      if(pwmLed1 < 255)
        analogWrite(LED1, ++pwmLed1);
    }// Serial.println(pwmLed1);

    if(regiao2 > luminosidadeIdeal) {
      if(pwmLed2 > 0)  
        analogWrite(LED2, --pwmLed2);
    } if(regiao2 < luminosidadeIdeal) {
      if(pwmLed2 < 255)
        analogWrite(LED2, ++pwmLed2);
    }
    
    regiao1 = (analogRead(A1) + analogRead(A2)) / 2;      // media da leitura na regiao 1
    regiao2 = (analogRead(A3) + analogRead(A4)) / 2;      // media da leitura na regiao 2
    atual = millis();
  }
}

void Abertura() { 
  lcd.createChar(0, char1);    // caracteres do logo
  lcd.createChar(1, char2);
  lcd.createChar(2, char3);
  lcd.createChar(3, char4);
  lcd.createChar(4, contrabarra);
  
  lcd.setCursor(0, 0);         // imprime logo
  lcd.print("    ");
  lcd.write((uint8_t)0);
  lcd.write((uint8_t)4);
  lcd.print("/");
  lcd.write((uint8_t)1);
  lcd.write((uint8_t)2);
  lcd.write((uint8_t)4);
  lcd.print("/");
  lcd.write((uint8_t)3);
  lcd.setCursor(5, 1);
  lcd.print("unoSys");
  FadeLuz(BACKLIGHT, 1);
  
  
  // bloco para destruir todas as variaveis da musica apos a execucao
  {
    blocoDeMemoria = (note**) calloc(12, sizeof(note*));
    m.add_channel(&pinA5);
    m.add_channel(&pin12);
    m.add_channel(&pin13);
    Melodia(pin13, pin12, pinA5); // executa musica com parada
  } delay(100);
  DDRB &= B001111;         // desativa saida de som das portas de som (12,13)
  pinMode(A5, INPUT); //DDRC &= B011111;         // desativa saida de som das portas de som (A5)
  
  FadeLuz(BACKLIGHT, 0);   // exibicao do slogan
  lcd.setCursor(0, 0);
  lcd.print("      Home      ");
  lcd.setCursor(0, 1);
  lcd.print("   Automation   ");
  FadeLuz(BACKLIGHT, 1);
  delay(700);
  FadeLuz(BACKLIGHT, 0);
}

int LerBotao() {
  leituraBotao = analogRead(A0);
  if (leituraBotao < 50)   return btnDIR;
  if (leituraBotao < 195)  return btnCIMA;
  if (leituraBotao < 380)  return btnBAIXO;
  if (leituraBotao < 555)  return btnESQ;
  if (leituraBotao < 790)  return btnSELECT;
  return btnNENHUM;
}

void FadeLuz(int pino, int tipo) {
  if(tipo == 1) // subida
    for(int i = 0; i < 255; i++) {
      analogWrite(pino, i);
      delay(9);
    } else if(tipo == 0) // descida
    for(int i = 255; i > 0; i--) {
      analogWrite(pino, i);
      delay(9);
    }
}

void AlteraGrafico(int entrada) {
  int i, j;
  
  for(i = 0; i < 8; i++) // move bytes do primeiro caractere para a esquerda (bitshift)
    char1[i] <<= 1;
  for(i = 0; i < 8; i++) // traz overflow do segundo caractere para o primeiro
    if(char2[i] >= 16) {
      char1[i]++;
      char2[i] -= 16;
    }
      
  for(i = 0; i < 8; i++) // e assim sucessivamente...
    char2[i] <<= 1;
  for(i = 0; i < 8; i++)
    if(char3[i] >= 16) {
      char2[i]++;
      char3[i] -= 16;
    }
      
  for(i = 0; i < 8; i++) // ...
    char3[i] <<= 1;
  for(i = 0; i < 8; i++)
    if(char4[i] >= 16) {
      char3[i]++;
      char4[i] -= 16;
    }
      
  for(i = 0; i < 8; i++) // no ultimo caractere, faz o bitshift
    char4[i] <<= 1; 
  for(i = 0; i <= 8; i++) // acrescenta na ultima coluna do ultimo caractere a entrada atual
    if(entrada == i)
      for(j = 0; j < i; j++)
        char4[7-j]++;
        
  lcd.createChar(0, char1); // atualiza conjunto de caracteres
  lcd.createChar(1, char2);
  lcd.createChar(2, char3);
  lcd.createChar(3, char4);
}

void ImprimeSegundaLinha() {
  lcd.setCursor(0, 1);            // imprime ilum. LCD
  if(configFundo == 0) {
    lcd.write((uint8_t)4);
    lcd.print("   ");
  } else if(configFundo == 1) {
    lcd.write((uint8_t)4);
    lcd.write((uint8_t)5);
    lcd.print("  ");
  } else if(configFundo == 2) {
    lcd.write((uint8_t)4);
    lcd.write((uint8_t)5);
    lcd.write((uint8_t)6);
    lcd.print(" ");
  } else if(configFundo == 3) {
    lcd.write((uint8_t)4);
    lcd.write((uint8_t)5);
    lcd.write((uint8_t)6);
    lcd.write((uint8_t)7);
  } else lcd.print("Auto");
  
  lcd.print("  ");                 // imprime iluminacao
  if(configLuz == 0) {
    lcd.write((uint8_t)4);
    lcd.print("   ");
  } else if(configLuz == 1) {
    lcd.write((uint8_t)4);
    lcd.write((uint8_t)5);
    lcd.print("  ");
  } else if(configLuz == 2) {
    lcd.write((uint8_t)4);
    lcd.write((uint8_t)5);
    lcd.write((uint8_t)6);
    lcd.print(" ");
  } else if(configLuz == 3) {
    lcd.write((uint8_t)4);
    lcd.write((uint8_t)5);
    lcd.write((uint8_t)6);
    lcd.write((uint8_t)7);
  } else if(configLuz == 4)
    lcd.print("Auto");
  else lcd.print("Off ");
  
  lcd.print(" ");                   // imprime grafico
  lcd.print((char)0x7E);            // setinha
  lcd.write((uint8_t)0);
  lcd.write((uint8_t)1);
  lcd.write((uint8_t)2);
  lcd.write((uint8_t)3);
}

void ProcessaEntrada() {
  botoeira = !digitalRead(BOTAO);
  valorLido = LerBotao(); // le entrada do botao em valorLido
  
  if(configModo == 2) {
    if(results.value == BAIXO || valorLido == btnBAIXO)  // configura luz
      if(luminosidadeIdeal > 0)
        luminosidadeIdeal -= 20;
    if(results.value == CIMA || valorLido == btnCIMA)
      if(luminosidadeIdeal < 1023)
        luminosidadeIdeal += 20;
    if(results.value == ESQUERDA || valorLido == btnESQ)  // configura luz
      if(luminosidadeIdeal > 0)
        luminosidadeIdeal -= 5;
    if(results.value == DIREITA || valorLido == btnDIR)
      if(luminosidadeIdeal < 1023)
        luminosidadeIdeal += 5;
    
    if(luminosidadeIdeal > 1023)
      luminosidadeIdeal = 1023;
    if(luminosidadeIdeal < 0)
      luminosidadeIdeal = 0;
  } else {
    if(results.value == BAIXO || valorLido == btnBAIXO)   // configura luz
      if(configLuz > -1)
        configLuz--;
    if(results.value == CIMA || valorLido == btnCIMA)
      if(configLuz < 4)
        configLuz++;  
    if(results.value == ESQUERDA || valorLido == btnESQ)  // configura fundo
      if(configFundo > 0)
        configFundo--;
    if(results.value == DIREITA || valorLido == btnDIR)
      if(configFundo < 4)
        configFundo++;
  }

  if(results.value == SELECT || valorLido == btnSELECT) { // configura modo
    if(configModo < 2)
      configModo++;
    else if(configModo == 2)
      configModo = 0;
  }
    
  if(botoeira) {
    if(configLuz < 4)
      configLuz++;
    else
      configLuz = -1;
    delay(150);
  }
    
  if(configLuz != 4)
    delay(150); // realizar paradas de 150ms para compensar ausencia da configuracao automatica
}

inline void Melodia(channel &melodia, channel &segundaVoz, channel &baixo) {
  note g1;
  g1.tone = -2;
  g1.duration = EIGHTH * 4;

  note c;
  c.tone = 3;
  c.duration = EIGHTH;

  note e;
  e.tone = 7;
  e.duration = EIGHTH;

  note g2;
  g2.tone = 10;
  g2.duration = EIGHTH * 3;

  note fs;
  fs.tone = -3;
  fs.duration = EIGHTH;

  note b1;
  b1.tone = 2;
  b1.duration = EIGHTH * 3;

  note b2;
  b2.tone = -10;
  b2.duration = EIGHTH * 4;

  note d;
  d.tone = -19;
  d.duration = EIGHTH;

  note g3;
  g3.tone = -14;
  g3.duration = EIGHTH * 3;

  note g4;
  g4.tone = -26;
  g4.duration = EIGHTH * 4;

  note rest;
  rest.tone = REST;
  rest.duration = EIGHTH;

  melodia.notes = blocoDeMemoria;
  segundaVoz.notes =  melodia.notes + 12;
  baixo.notes = segundaVoz.notes + 12;
  
  melodia.realloc_notes();
  segundaVoz.realloc_notes();
  baixo.realloc_notes();

  melodia.queue(&e);
  melodia.queue(&e);
  melodia.queue(&rest);
  melodia.queue(&e);
  melodia.queue(&rest);
  melodia.queue(&c);
  melodia.queue(&e);
  melodia.queue(&rest);
  melodia.queue(&g2);
  melodia.queue(&rest);
  melodia.queue(&g1);

  segundaVoz.queue(&fs);
  segundaVoz.queue(&fs);
  segundaVoz.queue(&rest);
  segundaVoz.queue(&fs);
  segundaVoz.queue(&rest);
  segundaVoz.queue(&fs);
  segundaVoz.queue(&fs);
  segundaVoz.queue(&rest);
  segundaVoz.queue(&b1);
  segundaVoz.queue(&rest);
  segundaVoz.queue(&b2);

  baixo.queue(&d);
  baixo.queue(&d);
  baixo.queue(&rest);
  baixo.queue(&d);
  baixo.queue(&rest);
  baixo.queue(&d);
  baixo.queue(&d);
  baixo.queue(&rest);
  baixo.queue(&g3);
  baixo.queue(&rest);
  baixo.queue(&g4);

  note stop;
  stop.tone = STOP;
  stop.duration = 0;
  melodia.queue(&stop);

  m.play();
}

void InfravermelhoSerial() {
  Serial.print(results.decode_type);
  Serial.print(", ");
  Serial.print(results.panasonicAddress);
  Serial.print(", ");
  Serial.print(results.value);
  Serial.print(", ");
  Serial.print(results.bits);
  Serial.print(", ");
  Serial.println(results.rawlen);
}
