Pill Tracker

## Secrets Setup (WiFi and Future Credentials)

This project supports a public-safe secrets pattern:

- `include/secrets_template.h` is committed and contains dummy values.
- `include/secrets.h` contains real credentials and is gitignored.

Build behavior:

- If `include/secrets.h` exists, it is used.
- If missing, `include/secrets_template.h` is used and compile-time warning is emitted.

### First-time local setup

1. Copy `include/secrets_template.h` to `include/secrets.h`.
2. Replace WiFi entries with your real credentials.
3. Keep `include/secrets.h` local only (already ignored by `.gitignore`).

### Runtime diagnostics

When `SERIAL_DEBUG_ENABLE` is set to 1, startup logs show:

- whether `secrets.h` or template is active
- imported WiFi SSIDs and passwords loaded into `WiFiMulti`

### Credential history note

This repository has historical commits that previously contained credentials.
Treat those credentials as exposed and rotate them. For clean public history,
create a new remote repository using this secrets workflow.


<img width="480" height="640" alt="image5" src="https://github.com/user-attachments/assets/e945c30b-efc6-4073-bdb6-733a573644cc" />
<img width="480" height="640" alt="image1" src="https://github.com/user-attachments/assets/0dca09cb-033f-43f9-8d06-340f6578176e" />
<img width="480" height="640" alt="image0" src="https://github.com/user-attachments/assets/e110b6e4-567a-483c-b682-f6cfb4341464" />
<img width="480" height="640" alt="image2" src="https://github.com/user-attachments/assets/01123a2e-db13-4e1b-8a91-66e263ec7f33" />
<img width="480" height="640" alt="image3" src="https://github.com/user-attachments/assets/50be9aca-6439-40df-a739-13edbb70360d" />
<img width="480" height="640" alt="image4" src="https://github.com/user-attachments/assets/a1962dff-b46f-463d-80a5-ebc806b89117" />
<img width="480" height="640" alt="image6" src="https://github.com/user-attachments/assets/341939b7-b1cd-4499-86ed-4cca027a090d" />
<img width="480" height="640" alt="image7" src="https://github.com/user-attachments/assets/91ef445e-3168-40b5-b8c8-9c6bbc2b895b" />
<img width="480" height="640" alt="image8" src="https://github.com/user-attachments/assets/2f580317-bb8a-41c6-8da7-17133de30dfd" />
