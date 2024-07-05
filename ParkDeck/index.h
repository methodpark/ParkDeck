const char mainPage[] PROGMEM = R"=====(
<HTML>
    <HEAD>
        <TITLE>ParkDeck Auth Flow</TITLE>
    </HEAD>
    <BODY>
        <CENTER>
            <B>Click the following link to connect your ParkDeck and </B>
            <a href="https://accounts.spotify.com/authorize?response_type=code&client_id=%s&redirect_uri=%s&scope=user-modify-playback-state user-read-currently-playing user-read-playback-state user-library-modify user-library-read">login to Spotify</a>
        </CENTER>
    </BODY>
</HTML>
)=====";

const char errorPage[] PROGMEM = R"=====(
<HTML>
    <HEAD>
        <TITLE>ParkDeck Auth Flow</TITLE>
    </HEAD>
    <BODY>
        <CENTER>
            <B>Click the following link to connect your ParkDeck and </B>
            <a href="https://accounts.spotify.com/authorize?response_type=code&client_id=%s&redirect_uri=%s&scope=user-modify-playback-state user-read-currently-playing user-read-playback-state user-library-modify user-library-read">login to Spotify</a>
        </CENTER>
    </BODY>
</HTML>
)=====";
