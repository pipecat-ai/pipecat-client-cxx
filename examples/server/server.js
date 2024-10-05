import express from "express";
import dotenv from "dotenv";
import cors from "cors";

dotenv.config({
  path: ".env.local",
  override: true,
});

const app = express();
app.use(cors());
app.use(express.json());

app.post("/connect", async (req, res) => {
  const { services, config } = req.body;

  if (!services || !config || !process.env.DAILY_BOTS_URL) {
    return res
      .status(400)
      .json({ info: "Services or config not found on request body" });
  }

  const payload = {
    bot_profile: "voice_2024_08",
    max_duration: 600,
    services,
    config: [...config],
  };

  const request = await fetch(process.env.DAILY_BOTS_URL, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Authorization: `Bearer ${process.env.DAILY_BOTS_API_KEY}`,
    },
    body: JSON.stringify(payload),
  });

  const response = await request.json();

  if (request.status !== 200) {
    return res.status(request.status).json({ info: response.error });
  }

  res.json(response);
});

app.listen(3000, () => console.log("Server is listening on port 3000..."));
