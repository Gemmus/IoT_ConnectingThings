const express = require('express');
const app = express();
const port = 3000;

app.use(express.static(__dirname)); // Serving static files

app.get('/', (req, res) => {
    res.sendFile(__dirname + '/index.html');
});

app.get('/:name', (req, res) => {
    const numberInput = parseInt(req.query.num);
    if (isNaN(numberInput) || numberInput < 1 || numberInput > 10) {
        const name = req.query.fname.trim();
        res.send(`Hey, ${name} !!! You need to do a better job of reading instructions!!! The number that I am thinking of is between 1 - 10!!!`);
    } else {
        const name = req.params.name;
        const randomNumber = Math.floor(Math.random() * 10) + 1;
        if (numberInput === randomNumber) {
            res.send(`Excellent ${name} , You chose the correct value of... ${numberInput}. You might just have ESP!!!`);
        } else {
            res.send(`Sorry ${name} , Unfortunately the number that I was thinking of was ${randomNumber}`);
        }
    }
});

app.get('*', (req, res) => {
    res.redirect('/');
});

app.listen(port, () => {
    console.log(`Server running at http://localhost:${port}`);
});
