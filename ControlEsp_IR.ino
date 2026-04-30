#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "ESP8266_LED";
const char* password = "12345678";  // mínimo 8 caracteres

// Variável inicializada com um valor padrão para não começar vazia
int temperatura = 25; 

ESP8266WebServer server(80);

bool ledState = false; // false = desligado, true = ligado

String getHTML() {
  String html = "<!DOCTYPE html><html lang='pt-BR'>";
  html += "<head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Controle ESP8266</title>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;background:#f2f2f2;margin-top:30px;}";
  html += "h1{color:#333;}";
  html += ".status{font-size:20px;margin:15px;}";
  html += ".on{color:green;font-weight:bold;}";
  html += ".off{color:red;font-weight:bold;}";
  html += "button{padding:15px 30px;font-size:18px;margin:10px;border:none;border-radius:10px;cursor:pointer;}";
  html += ".btn-on{background:#4CAF50;color:white;}";
  html += ".btn-off{background:#f44336;color:white;}";
  html += ".btn-temp{background:#2196F3;color:white;}";
  html += ".temp-input{padding:12px;font-size:18px;border-radius:8px;border:1px solid #ccc;width:80px;text-align:center;margin-right:10px;}";
  html += ".section{background:white;padding:20px;border-radius:15px;display:inline-block;margin:15px;box-shadow:0px 0px 10px rgba(0,0,0,0.1); width: 300px;}";
  html += "</style></head><body>";
  
  html += "<h1>Painel de Controle - ESP8266</h1>";

  // --- SEÇÃO DO LED ---
  html += "<div class='section'>";
  html += "<h2>Controle do LED</h2>";
  html += "<div class='status'>Status: ";
  html += ledState ? "<span class='on'>LIGADO</span>" : "<span class='off'>DESLIGADO</span>";
  html += "</div>";
  html += "<a href='/ligar'><button class='btn-on'>Ligar</button></a>";
  html += "<a href='/desligar'><button class='btn-off'>Desligar</button></a>";
  html += "</div>";

  // --- SEÇÃO DA TEMPERATURA ---
  html += "<div class='section'>";
  html += "<h2>Ajuste de Temperatura</h2>";
  html += "<div class='status'>Atual: <strong>" + String(temperatura) + " &deg;C</strong></div>";
  // O formulário envia um GET para a rota /temperatura com o parâmetro 'valor'
  html += "<form action='/temperatura' method='GET'>";
  html += "<input type='number' name='valor' class='temp-input' value='" + String(temperatura) + "' min='0' max='100'>";
  html += "<button type='submit' class='btn-temp'>Definir</button>";
  html += "</form>";
  html += "</div>";

  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleLigar() {
  digitalWrite(LED_BUILTIN, LOW); // ativo em LOW
  ledState = true;
  server.send(200, "text/html", getHTML());
}

void handleDesligar() {
  digitalWrite(LED_BUILTIN, HIGH); // desligado
  ledState = false;
  server.send(200, "text/html", getHTML());
}

// --- NOVA FUNÇÃO HANDLE TEMPERATURA ---
void handletemperatura() {
  // Verifica se o formulário enviou o parâmetro "valor"
  if (server.hasArg("valor")) { 
    // Captura a string enviada, converte para inteiro e salva na variável global
    temperatura = server.arg("valor").toInt(); 
    
    Serial.print("Nova temperatura definida via Web: ");
    Serial.println(temperatura);
  }
  // Recarrega a página atualizada
  server.send(200, "text/html", getHTML()); 
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // inicia desligado

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("Modo AP iniciado");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP do AP: ");
  Serial.println(WiFi.softAPIP());

  // Mapeamento das Rotas
  server.on("/", handleRoot);
  server.on("/ligar", handleLigar);
  server.on("/desligar", handleDesligar);
  server.on("/temperatura", handletemperatura); // Rota para processar a temperatura

  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();
}