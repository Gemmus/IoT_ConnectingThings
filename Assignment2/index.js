const express = require('express');
const mqtt = require('mqtt');
const path = require('path');
const util = require('util');
const fs = require('fs');
const bodyParser = require('body-parser');
const app = express();

const readFile = util.promisify(fs.readFile);
const writeFile = util.promisify(fs.writeFile);
// Create variables for MQTT use here

app.use(bodyParser.json());
function read(filePath = './message.json') {
    return readFile(path.resolve(__dirname, filePath)).then(data => JSON.parse(data));
}
function write(data, filePath = './message.json') {
    return writeFile(path.resolve(__dirname, filePath), JSON.stringify(data));
}

// create an MQTT instance
const ssid = "KME662";
const password = "SmartIot";
const broker_address = "mqtt://192.168.1.254";

const options = {
    clientId: 'gemma-server',
    username: ssid,
    password: password
}

const mqttClient = mqtt.connect(broker_address, options);

// Check that you are connected to MQTT and subscribe to a topic (connect event)
mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');
    mqttClient.subscribe('metro-messenger', (err) => {
        if (err) {
            console.error('Failed to subscribe to MQTT topic:', err);
        } else {
            console.log('Subscribed to MQTT topic: metro-messenger');
        }
    });
});

// Handle instance where MQTT will not connect (error event)
mqttClient.on('error', (err) => {
    console.error('MQTT error:', err);
});

// Handle when a subscribed message comes in (message event)
mqttClient.on('message', (topic, message) => {
    console.log('Received message:', message.toString());
    // Handle incoming messages here, you may want to update the message list
    // and send updates to connected clients via WebSockets or other means
});

app.use(bodyParser.json());

// Route to serve the home page
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

// Route to serve the JSON array from the file message.json when requested from the home page
app.get('/messages', async (req, res) => {
    try {
        const messages = await read();
        res.json(messages);
    } catch (error) {
        console.error('Error reading messages:', error);
        res.status(500).send('Internal Server Error');
    }
});

// Route to serve the page to add a message
app.get('/add', (req, res) => {
    res.sendFile(path.join(__dirname, 'message.html'));
});

// Route to show a selected message. Note, it will only show the message as text. No html needed
app.get('/:id', async (req, res) => {
    try {
        const messages = await read();
        const message = messages.find(msg => msg.id === req.params.id);
        if (message) {
            res.send(JSON.stringify(message));
        } else {
            res.status(404).send('Message not found');
        }
    } catch (error) {
        console.error('Error reading message:', error);
        res.status(500).send('Internal Server Error');
    }
});

// Route to CREATE a new message on the server and publish to mqtt broker
app.post('/', async (req, res) => {
    try {
        const { topic, msg } = req.body;
        const newMessage = {
            id: Math.random().toString(36).substring(2, 8),
            msg
        };
        const messages = await read() || [];
        messages.push(newMessage);
        await write(messages);

        // Publish the new message to MQTT broker
        mqttClient.publish('metro-messenger', JSON.stringify(newMessage));

        res.sendStatus(200);
    } catch (error) {
        console.error('Error creating message:', error);
        res.status(500).send('Internal Server Error');
    }
});

// Route to delete a message by id (Already done for you)
app.delete('/:id', async (req, res) => {
    try {
        const messages = await read();
        await write(messages.filter(c => c.id !== req.params.id));
        res.sendStatus(200);
    } catch (e) {
        res.sendStatus(200);
    }
});

// listen to the port
app.listen(3000);
