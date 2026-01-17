import smtplib

server = smtplib.SMTP("localhost", 2525)
server.sendmail(
    "rohit@test.com",
    "user@local.com",
    """Subject: Python Test

Hello from Python SMTP client!
"""
)
server.quit()
