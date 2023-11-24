#include <Arduino.h>

void SendEmail(String htmlMsg, String MessageSubject)
{
    smtp.debug(1);
    smtp.callback(smtpCallback);
    ESP_Mail_Session session;
    session.server.host_name = SMTP_HOST;
    session.server.port = SMTP_PORT;
    session.login.email = AUTHOR_EMAIL;
    session.login.password = AUTHOR_PASSWORD;
    session.time.ntp_server = F("pool.ntp.org,time.nist.gov");
    session.time.gmt_offset = -5;
    session.time.day_light_offset = 0;
    SMTP_Message message;
    message.sender.name = F("espBoilerMon");
    message.sender.email = AUTHOR_EMAIL;
    message.subject = MessageSubject;
    message.addRecipient(F("NAME OF RECIPIENT"), F("gunnar.norin@gmail.com")); // edit with the email address you are sending to
    message.html.content = htmlMsg;
    message.html.charSet = F("us-ascii");
    message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
    if (!smtp.connect(&session /* session credentials */))
        Serial.println("Error connecting to SMTP server, " + smtp.errorReason());
    if (!MailClient.sendMail(&smtp, &message))
    {
        Serial.println("Error sending Email, " + smtp.errorReason());
    }
}

void smtpCallback(SMTP_Status status)
{
    Serial.println(status.info());
    if (status.success())
    {
        Serial.println("----------------");
        ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
        ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
        Serial.println("----------------\n");

        for (size_t i = 0; i < smtp.sendingResult.size(); i++)
        {
            SMTP_Result result = smtp.sendingResult.getItem(i);
            ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
            ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
            ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
            ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
        }
        Serial.println("----------------\n");
        smtp.sendingResult.clear();
    }
}
