#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#define SMTP_SERVER "smtp://smtp.gmail.com:587"
#define SENDER_EMAIL "official.rohitsingh22@gmail.com" 
#define FROM_ADDR    "<" SENDER_EMAIL ">"
#define TO_ADDR      "<rohitvsingh2000@gmail.com>"
#define PASSWORD     "" 

char dynamic_to_header[100]; 

static const char *payload_text[] = {
  dynamic_to_header, 
  "From: " FROM_ADDR "\r\n",
  "Subject: SMTP Update for You\r\n",
  "\r\n", 
  "This is an automated update sent from AWS EC2.\r\n",
  "Have a great day!\r\n",
  NULL
};

struct upload_status {
  int lines_read;
};

static size_t payload_source(void *ptr, size_t size, size_t nmemb, void *userp) {
  struct upload_status *upload_ctx = (struct upload_status *)userp;
  const char *data;

  if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
    return 0;
  }

  data = payload_text[upload_ctx->lines_read];

  if(data) {
    size_t len = strlen(data);
    memcpy(ptr, data, len);
    upload_ctx->lines_read++;
    return len;
  }
  return 0;
}

int send_email_to(const char *target_email) {
  CURL *curl;
  CURLcode res = CURLE_OK;
  struct curl_slist *recipients = NULL;
  struct upload_status upload_ctx = { 0 };

  snprintf(dynamic_to_header, sizeof(dynamic_to_header), "To: <%s>\r\n", target_email);

  upload_ctx.lines_read = 0;

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, SMTP_SERVER);
    curl_easy_setopt(curl, CURLOPT_USERNAME, SENDER_EMAIL);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, PASSWORD);
    
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, FROM_ADDR);
    

    char formatted_recipient[100];
    snprintf(formatted_recipient, sizeof(formatted_recipient), "<%s>", target_email);
    recipients = curl_slist_append(recipients, formatted_recipient);
    
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

    printf("Sending to %s... ", target_email);
    res = curl_easy_perform(curl);

    if(res != CURLE_OK)
      fprintf(stderr, "FAILED: %s\n", curl_easy_strerror(res));
    else
      printf("OK!\n");

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
  }
  return (int)res;
}

int main(void) {

  const char *email_list[] = {
    "chinthanagbhat@gmail.com",
    "chaitu11106@gmail.com",
    "deekshakhathi26@gmail.com",
    "aditiladia14@gmail.com"
  };

  int count = sizeof(email_list) / sizeof(email_list[0]);

  printf("Starting bulk send to %d recipients...\n", count);

  for (int i = 0; i < count; i++) {
    send_email_to(email_list[i]);
  }

  printf("Done.\n");
  return 0;
}