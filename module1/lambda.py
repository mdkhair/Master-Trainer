import json
import base64
import boto3
from datetime import datetime
import logging

# Set up logging
logger = logging.getLogger()
logger.setLevel(logging.INFO)

s3 = boto3.client('s3')

def lambda_handler(event, context):
    logger.info("Received event: " + json.dumps(event))
    
    # Extract the Base64-encoded image from the incoming IoT message
    if 'picture' in event:
        base64_image = event['picture']
        
        # Decode the Base64 image
        try:
            image_data = base64.b64decode(base64_image)
        except Exception as e:
            logger.error("Error decoding Base64: " + str(e))
            return {
                'statusCode': 400,
                'body': json.dumps('Failed to decode image')
            }
        
        # Generate a unique filename for the image
        image_name = datetime.now().strftime('%Y%m%d_%H%M%S') + ".jpg"
        
        # Upload the image to S3
        try:
            s3.put_object(
                Bucket='my-espcam-images',  # Replace with your bucket name
                Key=image_name,
                Body=image_data,
                ContentType='image/jpeg'
            )
            logger.info(f"Image {image_name} successfully uploaded to S3")
        except Exception as e:
            logger.error("Error uploading to S3: " + str(e))
            return {
                'statusCode': 500,
                'body': json.dumps('Failed to upload image to S3')
            }
        
        return {
            'statusCode': 200,
            'body': json.dumps('Image successfully uploaded to S3')
        }

    return {
        'statusCode': 400,
        'body': json.dumps('No image found in the message')
    }
