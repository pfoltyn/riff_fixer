This project helps to create playable video files for a class of cheap mp3 players eg.:<br>
![olisky](https://github.com/user-attachments/assets/8e03b41f-e4e8-41b2-b3a0-15d877d5f081)

First, using ffmpeg, uncompress your video (in.mp4) to the rgb24 avi file (out.avi).<br>
Next use riff_edit.exe to generate file which is hopefully digestible by your mp3 player.<br>
- ffmpeg -i in.mp4 -vcodec rawvideo -pix_fmt rgb24 -s 208x176 -acodec adpcm_ima_wav -ac 1 -ar 22050 -r 21.681628 out.avi
- riff_edit.exe out.avi
