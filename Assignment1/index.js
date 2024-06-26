const express = require('express');
const app = express();
const port = 3000;

app.use(express.static(__dirname));

app.get('/', (req, res) => {
    res.sendFile(__dirname + '/index.html');
});

app.get('/:name', (req, res) => {
    let numberInput = req.query.num;
    const name = req.params.name;
    if (numberInput === "NaN") {
        res.redirect('/');
    } else {
        numberInput = parseInt(numberInput);
        if (isNaN(numberInput) || numberInput < 1 || numberInput > 10) {
            res.send(`Hey, ${name} !!! You need to do a better job of reading instructions!!! The number that I am thinking of is between 1 - 10!!!`);
        } else {
            const randomNumber = Math.floor(Math.random() * 10) + 1;
            if (numberInput === randomNumber) {
                res.send(`Excellent ${name} , You chose the correct value of... ${numberInput}. You might just have ESP!!!`);
            } else {
                res.send(`Sorry ${name} , Unfortunately the number that I was thinking of was ${randomNumber}`);
            }
        }
    }
});

app.get('*', (req, res) => {
    res.redirect('/');
});

app.listen(port, () => {
    console.log(`Server running at http://localhost:${port}`);
});
