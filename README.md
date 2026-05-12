Project Title:
Pill Logger

<img width="480" height="640" alt="image0" src="https://github.com/user-attachments/assets/e110b6e4-567a-483c-b682-f6cfb4341464" />

Project Objective:
To track the administration of oral medications (pills) that are taken 
irregularily. Two objectives:
1. To ensure daily dosage does not exceed prescription.
  (to answer the question: "When did i last take a pill and how many have i taken today?")
- Acheived by counting pushbutton presses with each each pill taken. 
   Enabling "late catchup" if a pill was taken without button press. 
   (allow more than prescribed daily dosage) 
- displaying count of pills taken during the current day and the date 
  and time of the last pill taken. 

2. To project date that pills will be depleted and require renewal. 
(to answer the question: "When will the pills run out compared to when the pharmacy will refill?")
- Acheived by tracking the total number of pills remaining in the current prescription,
  and projecting the date of taking the last pill, given the prescribed dosage.
- Also comparing against Pharmacy Next Refill date - a fixed date calculated based on 
   last refill date, number of pills dispensed and the prescribed dosage. 

3. Providing a robust and reliable system that  meets objectives 1 and 2 
- acheived by:
        - utilizing deep sleep to extend the battery life
        - ensuring battery removal and power cycling will not loose critical data.
        - maintaining accurate real time clock synchornizing periodiocally with NTP server
        - providing confirmation of successful NTP synchronization
        - providing battery monitoring to enable timely battery changes


Project Use cases:
1. when a pill is taken: long press pushbutton - confirmation 
   - calculate and record: pills_taken_today_count, pills_remaining_count, pills_depleted_date
   pills_depleted_date is the projected future date when pills_remaining_count will be zero 
   based on pills_remaining_count, Rx_pills_per_day and current date

2. when prescription is refilled - set Rx_last_refill_date, set Rx_pills_per_day, set RX_PILLS_DISPENSED_COUNT, set Rx_pills_dispensed_count and reset same value to pill_remaining_count, 
  (refill date does not change regardless of how pills are taken) 
  Rx_pills_per_day is the prescribed dosage but depending on actual meals eaten, actual dosing may be more or less depending on meals eaten
  the value is used to project Rx_next_refill_date, (The date the pharmacy will refill the prescription) AND ONLY CALCULATED AFTER THE Rx_last_refill_date IS UPDATED 
  and pills_depleted_date (the date the pills will actually be depleted based on the actual irregular pill consumption) AND CALCULATED EVERYTIME pill_remaining_count IS
  RESET, OR  DECREMENTED BY PB_TOP - LONG BUTTON PRESS)
  Also if a pill was taken when the device is not available (ie at a restaurant, then to maintain a correct count a delayed press will be required)

3. when pill_remaining_count must be updated manually
   - set pill_remaining_count, update(calculate)  pills_depleted_date

4. checking last pill taken and pill count taken current day 
   - display last_pill_taken_timestamp, pills_taken_today_count

5. checking for prescription renewal
   - display Rx_next_refill_date, pills_remaining, pills_depleted_date

6. checking battery health:
   - display current battery voltage. capacity indicator (in %) amd condition status (text)

7. checking NTP status:
   - display  last_NTP_Update_timestamp, last_NTP_attemp (success, fail, cause_code) (text)   


===============================================================
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


<img width="480" height="640" alt="image5" src="https://github.com/user-attachments/assets/e945c30b-efc6-4073-bdb6-733a573644cc" />
<img width="480" height="640" alt="image1" src="https://github.com/user-attachments/assets/0dca09cb-033f-43f9-8d06-340f6578176e" />
<img width="480" height="640" alt="image2" src="https://github.com/user-attachments/assets/01123a2e-db13-4e1b-8a91-66e263ec7f33" />
<img width="480" height="640" alt="image3" src="https://github.com/user-attachments/assets/50be9aca-6439-40df-a739-13edbb70360d" />
<img width="480" height="640" alt="image4" src="https://github.com/user-attachments/assets/a1962dff-b46f-463d-80a5-ebc806b89117" />
<img width="480" height="640" alt="image6" src="https://github.com/user-attachments/assets/341939b7-b1cd-4499-86ed-4cca027a090d" />
<img width="480" height="640" alt="image7" src="https://github.com/user-attachments/assets/91ef445e-3168-40b5-b8c8-9c6bbc2b895b" />
<img width="480" height="640" alt="image8" src="https://github.com/user-attachments/assets/2f580317-bb8a-41c6-8da7-17133de30dfd" />
