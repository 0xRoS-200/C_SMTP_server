# AWS Simple SMTP Server (C Implementation)

A guide to deploying a raw C-based SMTP server on AWS EC2 to receive real emails from Gmail/Outlook.

## Prerequisites
1.  **AWS Account** (Free Tier eligible).
2.  **Domain Name** (GoDaddy, Namecheap, etc.) hooked up to Cloudflare (optional but recommended).

---

## Phase 1: Launch the Server (AWS EC2)
1.  **Login to AWS Console** and search for **EC2**.
2.  Click **Launch Instance**.
3.  **Name:** `SMTP-Server`.
4.  **OS Image:** **Ubuntu Server 24.04 LTS** (Free Tier Eligible).
5.  **Instance Type:** **t2.micro** or **t3.micro** (Free Tier Eligible).
6.  **Key Pair:** Create new key pair (`smtp-key.pem`), download it, and keep it safe.
7.  **Network Settings:** Check "Create security group" and allow SSH (Port 22).
8.  Click **Launch Instance**.

---

## Phase 2: Open Port 25 (The Firewall)
AWS blocks email traffic by default. You must open the door.

1.  In EC2 Dashboard, click your **Instance ID**.
2.  Go to the **Security** tab -> Click the **Security Group** link (`sg-xxxx`).
3.  Click **Edit inbound rules** -> **Add rule**.
    * **Type:** `SMTP` (or Custom TCP).
    * **Port range:** `25`.
    * **Source:** `Anywhere-IPv4` (`0.0.0.0/0`).
4.  Click **Save rules**.

---

## Phase 3: Server Configuration
Connect to your server using **EC2 Instance Connect** (Browser) or SSH.

### 1. Update & Install GCC (Compiler)
```bash
sudo apt update
sudo apt install gcc -y