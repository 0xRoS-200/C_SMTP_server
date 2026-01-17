import smtplib

server = smtplib.SMTP('localhost', 2525)
server.set_debuglevel(1) # See the conversation in console

server.sendmail(
    "me@test.com", 
    "you@test.com", 
    "Subject: Hello\n\nThis is a test email sent to your C server."
)

server.quit()