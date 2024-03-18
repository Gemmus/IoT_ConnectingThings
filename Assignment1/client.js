document.getElementById('guessForm').addEventListener('submit', function(event) {
    event.preventDefault(); // Prevent the default form submission

    const name = document.getElementById('getName').value;
    const number = parseInt(document.getElementById('getNumber').value);

    // Validate number & send request to server
    if (number < 1 || number > 10) {
        window.location.href = `http://localhost:3000/name?fname=${name}+&number=${number}`;
    } else {
        window.location.href = `http://localhost:3000/${name}?num=${number}`;
    }
});
