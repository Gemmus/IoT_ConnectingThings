document.getElementById('guessForm').addEventListener('submit', function(event) {
    event.preventDefault(); // Prevent the default form submission

    const name = document.getElementById('getName').value;
    const number = document.getElementById('getNumber').value;

    window.location.href = `http://localhost:3000/${name}?num=${number}`;
});
