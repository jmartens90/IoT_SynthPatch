# Recipe for the backend image: start from a small official Python image,
# install the two libraries, copy our code, and say how to start it.
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
CMD ["python", "-u", "main.py"]
